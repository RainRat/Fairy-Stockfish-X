#!/bin/bash
# regression test variant bench numbers
# arguments: ./old_engine ./new_engine "bench_args" [variant1 variant2 variant3 ...]
# examples:
#   Pure bench: ./regression.sh ./old_engine ./new_engine "" chess xiangqi shogi
#   Perft bench: ./regression.sh ./old_engine ./new_engine "16 1 5 default perft" chess xiangqi shogi
#   Auto-detect common variants: ./regression.sh ./old_engine ./new_engine ""

set -euo pipefail
failed=0
if [[ $# -lt 3 ]]; then echo "usage: $0 old_engine new_engine bench_args [variants...]" >&2; exit 2; fi

echo "variant $1 $2"
# Convert bench_args to an array to handle empty or multi-word arguments correctly
read -r -a bench_args_arr <<< "$3"

# If no variants provided, extract them from UCI output
if [ $# -eq 3 ]; then
    echo "No variants specified, extracting from UCI output..."
    
    # Extract variants from first engine
    variants1=$(echo "uci" | "$1" | grep "option name UCI_Variant" | grep -o 'var [^[:space:]]*' | sed 's/var //' | sort)
    
    # Extract variants from second engine
    variants2=$(echo "uci" | "$2" | grep "option name UCI_Variant" | grep -o 'var [^[:space:]]*' | sed 's/var //' | sort)
    
    # Find intersection of variants (common to both engines)
    variants=$(comm -12 <(echo "$variants1") <(echo "$variants2"))
    
    if [[ -z "$variants" ]]; then
        echo "No common variants found between $1 and $2" >&2
        exit 1
    fi
    echo "Common variants found: $(echo "$variants" | wc -w)"

    # Convert to array for processing
    if [[ -n "$variants" ]]; then
        mapfile -t variant_array <<< "$variants"
    else
        variant_array=()
    fi
else
    # Use provided variants
    variant_array=("${@:4}")
fi

if [[ ${#variant_array[@]} -eq 0 ]]; then echo "No variants to compare" >&2; exit 1; fi

for var in "${variant_array[@]}"
do
    if ! ref_output=$(timeout "${FSX_REGRESSION_PROBE_TIMEOUT:-15m}" "$1" bench "$var" "${bench_args_arr[@]}" 2>&1); then
        echo "${var} reference engine probe failed or timed out: $1" >&2
        failed=1
        continue
    fi
    if ! new_output=$(timeout "${FSX_REGRESSION_PROBE_TIMEOUT:-15m}" "$2" bench "$var" "${bench_args_arr[@]}" 2>&1); then
        echo "${var} new engine probe failed or timed out: $2" >&2
        failed=1
        continue
    fi
    ref=$(printf '%s\n' "$ref_output" | awk '/Nodes searched  : / {print $4; exit}')
    signature=$(printf '%s\n' "$new_output" | awk '/Nodes searched  : / {print $4; exit}')
    if [ -z "$ref" ]; then
        echo "${var} none ${signature:-none} <-- missing reference result"
        failed=1
    elif [ -z "$signature" ]; then
        echo "${var} ${ref} none <-- missing new result"
        failed=1
    elif [ "$ref" != "$signature" ]; then
        echo "${var} ${ref} ${signature} <-- mismatch"
        failed=1
    else
        echo "${var} ${ref} OK"
    fi
done

exit "$failed"

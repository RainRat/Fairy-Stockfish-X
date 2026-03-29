#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include "../src/nnue/layers/affine_transform.h"

using namespace Stockfish::Eval::NNUE;
using namespace Stockfish::Eval::NNUE::Layers;

template <IndexType Dims>
struct DummyLayer {
    using OutputType = std::uint8_t;
    static constexpr IndexType OutputDimensions = Dims;
    static constexpr std::size_t BufferSize = 0;

    static constexpr std::uint32_t get_hash_value() { return 0; }
    bool read_parameters(std::istream&) { return true; }
    bool write_parameters(std::ostream&) const { return true; }
    const OutputType* propagate(const TransformedFeatureType* input, char*) const { return input; }
};

template <typename T>
void write_le(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <IndexType InDims, IndexType OutDims>
std::array<std::int32_t, OutDims> run_case(
    const std::array<std::uint8_t, InDims>& input,
    const std::array<std::int32_t, OutDims>& biases,
    const std::vector<std::int8_t>& weights)
{
    using Layer = AffineTransform<DummyLayer<InDims>, OutDims>;
    constexpr IndexType Padded = Layer::PaddedInputDimensions;

    std::stringstream params(std::ios::in | std::ios::out | std::ios::binary);
    for (IndexType o = 0; o < OutDims; ++o)
        write_le(params, biases[o]);
    for (IndexType o = 0; o < OutDims; ++o)
        for (IndexType i = 0; i < Padded; ++i)
            write_le(params, weights[o * Padded + i]);

    Layer layer;
    params.seekg(0);
    if (!layer.read_parameters(params)) {
        std::cerr << "failed to read layer parameters\n";
        std::exit(1);
    }

    alignas(CacheLineSize) std::array<char, Layer::BufferSize> buffer{};
    const auto* output = layer.propagate(input.data(), buffer.data());

    std::array<std::int32_t, OutDims> actual{};
    std::copy(output, output + OutDims, actual.begin());
    return actual;
}

template <IndexType InDims, IndexType OutDims>
std::array<std::int32_t, OutDims> scalar_ref(
    const std::array<std::uint8_t, InDims>& input,
    const std::array<std::int32_t, OutDims>& biases,
    const std::vector<std::int8_t>& weights)
{
    using Layer = AffineTransform<DummyLayer<InDims>, OutDims>;
    constexpr IndexType Padded = Layer::PaddedInputDimensions;

    std::array<std::int32_t, OutDims> expected{};
    for (IndexType o = 0; o < OutDims; ++o) {
        std::int32_t sum = biases[o];
        for (IndexType i = 0; i < InDims; ++i)
            sum += static_cast<std::int32_t>(weights[o * Padded + i]) * input[i];
        expected[o] = sum;
    }
    return expected;
}

template <std::size_t N>
bool equal_arrays(const std::array<std::int32_t, N>& a, const std::array<std::int32_t, N>& b) {
    for (std::size_t i = 0; i < N; ++i)
        if (a[i] != b[i])
            return false;
    return true;
}

void test_saturation() {
    constexpr IndexType InDims = 16;
    constexpr IndexType OutDims = 16;
    using Layer = AffineTransform<DummyLayer<InDims>, OutDims>;
    constexpr IndexType Padded = Layer::PaddedInputDimensions;

    std::array<std::uint8_t, InDims> input{};
    input.fill(255);
    std::array<std::int32_t, OutDims> biases{};
    std::vector<std::int8_t> weights(OutDims * Padded, 0);
    for (IndexType o = 0; o < OutDims; ++o)
        for (IndexType i = 0; i < InDims; ++i)
            weights[o * Padded + i] = 64;

    const auto actual = run_case<InDims, OutDims>(input, biases, weights);
    const auto expected = scalar_ref<InDims, OutDims>(input, biases, weights);

    if (!equal_arrays(actual, expected)) {
        std::cerr << "saturation regression\n";
        std::cerr << "expected " << expected[0] << " got " << actual[0] << '\n';
        std::exit(1);
    }
}

void test_tail_chunk() {
    constexpr IndexType InDims = 20;
    constexpr IndexType OutDims = 16;
    using Layer = AffineTransform<DummyLayer<InDims>, OutDims>;
    constexpr IndexType Padded = Layer::PaddedInputDimensions;

    std::array<std::uint8_t, InDims> input{};
    input[16] = 1;
    input[17] = 2;
    input[18] = 3;
    input[19] = 4;

    std::array<std::int32_t, OutDims> biases{};
    std::vector<std::int8_t> weights(OutDims * Padded, 0);
    for (IndexType o = 0; o < OutDims; ++o)
        for (IndexType i = 16; i < InDims; ++i)
            weights[o * Padded + i] = 1;

    const auto actual = run_case<InDims, OutDims>(input, biases, weights);
    const auto expected = scalar_ref<InDims, OutDims>(input, biases, weights);

    if (!equal_arrays(actual, expected)) {
        std::cerr << "tail chunk regression\n";
        std::cerr << "expected " << expected[0] << " got " << actual[0] << '\n';
        std::exit(1);
    }
}

int main() {
    test_saturation();
    test_tail_chunk();
    return 0;
}

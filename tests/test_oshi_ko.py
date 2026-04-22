import subprocess

def test_fen():
    engine = subprocess.Popen(
        ['./stockfish', 'load', 'variants.ini'],
        cwd='src',
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    commands = "setoption name UCI_Variant value ko-oshi\nposition fen 5/5/AaB2/5/5 w - - 0 1\ngo depth 5\n"
    engine.stdin.write(commands)
    engine.stdin.flush()
    
    found_bestmove = False
    while not found_bestmove:
        line = engine.stdout.readline()
        if not line:
            break
        line = line.strip()
        print(line)
        if line.startswith('bestmove'):
            found_bestmove = True
            
    engine.stdin.write("quit\n")
    engine.stdin.flush()

if __name__ == '__main__':
    test_fen()

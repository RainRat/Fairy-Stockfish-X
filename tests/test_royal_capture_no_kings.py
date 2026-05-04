import os
import sys
import pyffish as sf

def test_royal_capture():
    cfg = """
[noroyal-capture:chess]
king = c:K
allowChecks = true
    """
    sf.load_variant_config(cfg)
    
    # Board with Commoners instead of Kings. Commoner at e1 and e8.
    # We put a white rook at e2 to capture the black commoner at e8.
    fen = "4c3/8/8/8/8/8/4R3/4C3 w - - 0 1"
    
    # Check what pieces are on board
    print("FEN from sf:", sf.get_fen("noroyal-capture", fen, []))
    
    # Check if e2e8 is legal. For royal capture to be legal, it must end the game.
    moves = sf.legal_moves("noroyal-capture", fen, [])
    print("Legal moves:", moves)
    
    # In some variants, capturing the royal piece might not be in the move list
    # if it's considered illegal due to "leaving king in check" (if the commoner is the king).
    # But if we capture it, it's game over.
    
    # Let's check if the move e2e8 is considered a capture of a royal piece.
    is_capture = sf.is_capture("noroyal-capture", fen, [], "e2e8")
    print("Is e2e8 a capture?", is_capture)
    
    # Verify game end
    is_end, result = sf.is_immediate_game_end("noroyal-capture", fen, ["e2e8"])
    print("Is e2e8 game end?", is_end, "Result:", result)
    assert is_end == True
    assert result != 0

if __name__ == "__main__":
    test_royal_capture()
    print("Test passed.")

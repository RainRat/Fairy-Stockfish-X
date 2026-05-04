import pyffish

# Test if the variant is correctly loaded.
try:
    fen = pyffish.start_fen("knightmate")
    print("Knightmate initial fen:", fen)
except Exception as e:
    print("Error:", e)

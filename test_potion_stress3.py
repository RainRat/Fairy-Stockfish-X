import subprocess
import time
import sys

variants_append = """
[potion_overflow]
fairy = chess
potions = true
freezePotion = P
potionDropOnOccupied = true
"""

# Let's write the test directly in test.py instead of running a separate script, since the plan says "add a test `test_potion_overflow` in `test.py`"

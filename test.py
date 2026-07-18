"""Compatibility entry point; binding tests live in tests/python."""
import unittest

from tests.python.test_pyffish_api import TestBindings, TestPublicAPI

if __name__ == "__main__":
    unittest.main()

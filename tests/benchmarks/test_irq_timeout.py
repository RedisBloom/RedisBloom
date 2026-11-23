"""IRQ timeout checks without modifying host affinity or connecting to AWS."""

from pathlib import Path
import unittest
from unittest.mock import patch

import configure_affinity as affinity


class IrqTimeoutTests(unittest.TestCase):
    def test_selection_can_succeed_after_ten_seconds(self):
        with patch.object(Path, 'read_text', side_effect=['1', '0']), \
                patch.object(affinity.time, 'monotonic', side_effect=[0, 11]), \
                patch.object(affinity.time, 'sleep') as sleep:
            selected, reserved, _ = affinity.select_isolated_cores(
                {'27': 0}, {0, 1, 2}, {i: {i} for i in range(3)}, 2)
        self.assertEqual(selected, [1, 2])
        self.assertEqual(reserved, {1, 2})
        sleep.assert_called_once_with(0.1)

    def test_unavailable_cores_fail_after_thirty_seconds(self):
        with patch.object(Path, 'read_text', return_value='1'), \
                patch.object(affinity.time, 'monotonic', side_effect=[0, 29, 30]), \
                patch.object(affinity.time, 'sleep') as sleep:
            with self.assertRaisesRegex(RuntimeError, 'Not enough IRQ-free physical cores'):
                affinity.select_isolated_cores({'27': 0}, {0, 1, 2}, {i: {i} for i in range(3)}, 2)
        sleep.assert_called_once_with(0.1)


if __name__ == '__main__':
    unittest.main()

"""Readable Penguin eight-input linear predictor example.

The predictor uses normalized samples 0..7 to estimate sample 15, the newest
value in the 16-sample window. It has eight weights and one bias.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field


WINDOW = 16
INPUTS = 8
LEARNING_RATE = 0.01
MIN_SCALE_C = 0.05


def normalize(window: list[float]) -> list[float]:
    """Apply the same per-window centering and scaling used by Penguin."""
    if len(window) != WINDOW:
        raise ValueError(f"expected {WINDOW} samples, received {len(window)}")
    center = sum(window) / WINDOW
    variance = sum((value - center) ** 2 for value in window) / WINDOW
    scale = max(math.sqrt(variance), MIN_SCALE_C)
    return [(value - center) / scale for value in window]


@dataclass
class LinearPredictor:
    """One linear neuron trained one window at a time."""

    weights: list[float] = field(default_factory=lambda: [0.0] * INPUTS)
    bias: float = 0.0

    def predict(self, window: list[float]) -> float:
        """Estimate normalized sample 15 from normalized samples 0..7."""
        return self.bias + sum(weight * value for weight, value in zip(self.weights, window[:INPUTS]))

    def absolute_error(self, window: list[float]) -> float:
        """Return the score used by Penguin's predictor component."""
        return abs(window[-1] - self.predict(window))

    def train_step(self, window: list[float]) -> float:
        """Move weights and bias toward the newest normalized sample."""
        prediction = self.predict(window)
        actual = window[-1]
        error = actual - prediction

        # For squared loss, this update is stochastic gradient descent with a
        # constant factor folded into the learning rate. Positive error raises
        # the prediction; negative error lowers it.
        self.bias += LEARNING_RATE * error
        for index in range(INPUTS):
            self.weights[index] += LEARNING_RATE * error * window[index]
        return abs(error)


def normal_temperature_window(phase: float) -> list[float]:
    """Create a smooth normal sequence for the standalone demonstration."""
    return [22.0 + 0.10 * math.sin(phase + index * 0.22) for index in range(WINDOW)]


def main() -> None:
    # A fixed seed makes the example repeatable. Production Penguin also uses
    # deterministic initial parameters, although its exact generator differs.
    generator = random.Random(7)
    model = LinearPredictor(weights=[generator.uniform(-0.05, 0.05) for _ in range(INPUTS)])

    # Online training: each normal window is seen, scored, and immediately used
    # for one update. No dataset or heap-heavy training framework is required.
    for _ in range(400):
        for phase in (0.0, 0.25, 0.5, 0.75, 1.0):
            model.train_step(normalize(normal_temperature_window(phase)))

    normal = normalize(normal_temperature_window(0.4))
    sudden_change_raw = normal_temperature_window(0.4)
    sudden_change_raw[-1] += 1.5
    sudden_change = normalize(sudden_change_raw)

    normal_error = model.absolute_error(normal)
    changed_error = model.absolute_error(sudden_change)
    if changed_error <= normal_error:
        raise AssertionError("the sudden change should have the larger prediction error")
    print(f"normal prediction error: {normal_error:.6f}")
    print(f"changed prediction error: {changed_error:.6f}")
    print(f"changed/normal ratio: {changed_error / max(normal_error, 1e-12):.2f}x")


if __name__ == "__main__":
    main()

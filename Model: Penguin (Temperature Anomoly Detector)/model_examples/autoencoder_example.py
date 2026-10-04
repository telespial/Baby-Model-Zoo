"""Readable Penguin 16-8-4-8-16 autoencoder example.

This is an educational implementation using only the Python standard library.
The production MCU code uses fixed-size C arrays and float arithmetic. This
example favors explicit intermediate values and comments over speed.
"""

from __future__ import annotations

import math
import random
from typing import Iterable


WINDOW = 16
LEARNING_RATE = 0.003
MIN_SCALE_C = 0.05


def normalize(window: list[float]) -> list[float]:
    """Center and scale one chronological 16-sample temperature window."""
    if len(window) != WINDOW:
        raise ValueError(f"expected {WINDOW} samples, received {len(window)}")

    center = sum(window) / WINDOW
    variance = sum((value - center) ** 2 for value in window) / WINDOW
    scale = max(math.sqrt(variance), MIN_SCALE_C)
    return [(value - center) / scale for value in window]


def relu(value: float) -> float:
    """ReLU keeps positive activations and replaces negative ones with zero."""
    return max(0.0, value)


def dense(weights: list[list[float]], biases: list[float], values: list[float], *, activate: bool) -> tuple[list[float], list[float]]:
    """Evaluate one fully connected layer and return pre/post activations."""
    before_activation = [
        bias + sum(weight * value for weight, value in zip(row, values))
        for row, bias in zip(weights, biases)
    ]
    after_activation = [relu(value) for value in before_activation] if activate else before_activation[:]
    return before_activation, after_activation


def make_matrix(outputs: int, inputs: int, generator: random.Random) -> list[list[float]]:
    """Create small deterministic starting weights like an embedded initializer."""
    return [[generator.uniform(-0.05, 0.05) for _ in range(inputs)] for _ in range(outputs)]


def outer(left: list[float], right: list[float]) -> list[list[float]]:
    """Return the outer product used to form a dense layer's weight gradient."""
    return [[left_value * right_value for right_value in right] for left_value in left]


def backpropagate(weights: list[list[float]], downstream: list[float], pre_activation: list[float]) -> list[float]:
    """Move a gradient backward through a weight matrix and a ReLU."""
    result: list[float] = []
    for input_index, value_before_relu in enumerate(pre_activation):
        gradient = sum(weights[output_index][input_index] * downstream[output_index] for output_index in range(len(downstream)))
        result.append(gradient if value_before_relu > 0.0 else 0.0)
    return result


def apply_update(weights: list[list[float]], biases: list[float], weight_gradient: list[list[float]], bias_gradient: list[float]) -> None:
    """Apply one gradient-descent step in place without allocating model state."""
    for output_index in range(len(weights)):
        for input_index in range(len(weights[output_index])):
            weights[output_index][input_index] -= LEARNING_RATE * weight_gradient[output_index][input_index]
        biases[output_index] -= LEARNING_RATE * bias_gradient[output_index]


class Autoencoder:
    """Four dense layers: 16 → 8 → 4 → 8 → 16."""

    def __init__(self, seed: int = 7) -> None:
        generator = random.Random(seed)
        self.w1, self.b1 = make_matrix(8, 16, generator), [0.0] * 8
        self.w2, self.b2 = make_matrix(4, 8, generator), [0.0] * 4
        self.w3, self.b3 = make_matrix(8, 4, generator), [0.0] * 8
        self.w4, self.b4 = make_matrix(16, 8, generator), [0.0] * 16

    def forward(self, values: list[float]) -> tuple[list[float], tuple[list[float], ...]]:
        """Reconstruct a window and retain activations needed for training."""
        z1, h1 = dense(self.w1, self.b1, values, activate=True)
        z2, h2 = dense(self.w2, self.b2, h1, activate=True)
        z3, h3 = dense(self.w3, self.b3, h2, activate=True)
        _, output = dense(self.w4, self.b4, h3, activate=False)
        return output, (values, z1, h1, z2, h2, z3, h3)

    def reconstruction_error(self, values: list[float]) -> float:
        """Mean squared difference between input and reconstructed output."""
        output, _ = self.forward(values)
        return sum((actual - predicted) ** 2 for actual, predicted in zip(values, output)) / WINDOW

    def train_step(self, values: list[float]) -> float:
        """Run one complete forward/backward update and return pre-update MSE."""
        output, cache = self.forward(values)
        inputs, z1, h1, z2, h2, z3, h3 = cache

        # d/d(output) mean((output - input)^2)
        d4 = [2.0 * (predicted - actual) / WINDOW for predicted, actual in zip(output, inputs)]

        # Calculate every upstream delta before changing any weights. This is
        # conventional backpropagation and makes the dependency chain explicit.
        d3 = backpropagate(self.w4, d4, z3)
        d2 = backpropagate(self.w3, d3, z2)
        d1 = backpropagate(self.w2, d2, z1)

        apply_update(self.w4, self.b4, outer(d4, h3), d4)
        apply_update(self.w3, self.b3, outer(d3, h2), d3)
        apply_update(self.w2, self.b2, outer(d2, h1), d2)
        apply_update(self.w1, self.b1, outer(d1, inputs), d1)

        return sum((actual - predicted) ** 2 for actual, predicted in zip(inputs, output)) / WINDOW


def normal_temperature_window(phase: float) -> list[float]:
    """Create a gentle deterministic pattern used only for this demonstration."""
    return [22.0 + 0.08 * math.sin(phase + index * 0.28) for index in range(WINDOW)]


def train(model: Autoencoder, phases: Iterable[float], epochs: int) -> None:
    """Repeatedly present known-normal windows to the autoencoder."""
    training_windows = [normalize(normal_temperature_window(phase)) for phase in phases]
    for _ in range(epochs):
        for window in training_windows:
            model.train_step(window)


def main() -> None:
    model = Autoencoder(seed=7)
    train(model, phases=(0.0, 0.3, 0.6, 0.9), epochs=500)

    normal = normalize(normal_temperature_window(0.45))
    sudden_change_raw = normal_temperature_window(0.45)
    sudden_change_raw[-1] += 1.5
    sudden_change = normalize(sudden_change_raw)

    normal_error = model.reconstruction_error(normal)
    changed_error = model.reconstruction_error(sudden_change)
    if changed_error <= normal_error:
        raise AssertionError("the changed window should reconstruct less accurately")
    print(f"normal reconstruction error: {normal_error:.6f}")
    print(f"changed reconstruction error: {changed_error:.6f}")
    print(f"changed/normal ratio: {changed_error / max(normal_error, 1e-12):.2f}x")


if __name__ == "__main__":
    main()

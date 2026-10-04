"""Dependency-free BabyTempAE reference for reproducible host vectors."""
import math

WINDOW, H1, LATENT, H3 = 16, 8, 4, 8

def _uniform(seed):
    state = seed & 0xFFFFFFFF
    while True:
        state = (1664525 * state + 1013904223) & 0xFFFFFFFF
        yield ((state / 0x100000000) * 2.0) - 1.0

class BabyTempAE:
    def __init__(self, seed=7):
        random = _uniform(seed)
        self.w1 = [[next(random) * 0.05 for _ in range(H1)] for _ in range(WINDOW)]
        self.b1 = [0.0] * H1
        self.w2 = [[next(random) * 0.05 for _ in range(LATENT)] for _ in range(H1)]
        self.b2 = [0.0] * LATENT
        self.w3 = [[next(random) * 0.05 for _ in range(H3)] for _ in range(LATENT)]
        self.b3 = [0.0] * H3
        self.w4 = [[next(random) * 0.05 for _ in range(WINDOW)] for _ in range(H3)]
        self.b4 = [0.0] * WINDOW

    @staticmethod
    def _relu(values): return [max(0.0, value) for value in values]

    @staticmethod
    def _dense(values, weights, biases):
        return [bias + sum(values[i] * weights[i][j] for i in range(len(values)))
                for j, bias in enumerate(biases)]

    def forward(self, values):
        h1 = self._relu(self._dense(values, self.w1, self.b1))
        h2 = self._relu(self._dense(h1, self.w2, self.b2))
        h3 = self._relu(self._dense(h2, self.w3, self.b3))
        return self._dense(h3, self.w4, self.b4)

    def reconstruction_error(self, values):
        output = self.forward(values)
        return sum((a - b) ** 2 for a, b in zip(values, output)) / WINDOW

def normalize(window):
    center = sum(window) / len(window)
    scale = max(math.sqrt(sum((value - center) ** 2 for value in window) / len(window)), 0.05)
    return [(value - center) / scale for value in window], center, scale

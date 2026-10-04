"""Deterministic synthetic baseline and anomaly traces for host tests."""
import math

def normal_trace(count=512, start=22.0, drift=0.0005):
    return [start + drift * i + 0.02 * math.sin(i * 0.37) for i in range(count)]

def inject_spike(values, index, magnitude=3.0):
    result = list(values); result[index] += magnitude; return result

def inject_step(values, index, magnitude=2.0):
    return [value + (magnitude if i >= index else 0.0) for i, value in enumerate(values)]

def inject_ramp(values, index, slope=0.05):
    return [value + (i - index) * slope if i >= index else value for i, value in enumerate(values)]

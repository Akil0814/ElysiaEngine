"""Reproduce the low-gain PCM showcase fixtures; no external assets are used."""
from pathlib import Path
import math
import struct
import wave

DEST = Path(__file__).resolve().parents[1] / "assets/audio/showcase"
RATE = 22050

def generate(name, duration, frequencies):
    DEST.mkdir(parents=True, exist_ok=True)
    count = int(duration * RATE)
    samples = bytearray()
    for index in range(count):
        time = index / RATE
        envelope = min(1.0, time / 0.025, (duration - time) / 0.025)
        signal = sum(math.sin(2 * math.pi * frequency * time) for frequency in frequencies) / len(frequencies)
        samples.extend(struct.pack("<h", round(32767 * 0.08 * envelope * signal)))
    with wave.open(str(DEST / name), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(RATE)
        stream.writeframes(samples)

if __name__ == "__main__":
    generate("tone.wav", 0.3, [440])
    generate("loop_a.wav", 2.0, [220, 330, 440])
    generate("loop_b.wav", 2.0, [262, 330, 392])

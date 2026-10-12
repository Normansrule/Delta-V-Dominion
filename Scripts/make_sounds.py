"""Synthesizes the game's sound set into Art/Audio/*.wav (mono, 44.1 kHz, 16-bit).

Runs with ordinary Python 3 + numpy, outside Unreal:
  python Scripts/make_sounds.py
build_audio.py then imports them into /Game/Audio. Everything is generated (filtered noise, resonant
bodies, pitch sweeps and convolution-style tails), so there are no licensing questions; swap in
recorded/library sounds per file name later and the game picks them up unchanged.
"""

import os
import wave

import numpy as np

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art", "Audio")
rng = np.random.default_rng(1234)


def t_axis(seconds):
    return np.arange(int(SR * seconds)) / SR


def noise(seconds):
    return rng.standard_normal(int(SR * seconds))


def lowpass(x, cutoff):
    """Zero-phase 4th-order low-pass (cutoff in Hz), applied in the frequency domain."""
    n = len(x)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    return np.fft.irfft(np.fft.rfft(x) / (1.0 + (f / cutoff) ** 4), n)


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def env(seconds, attack, decay, curve=4.0):
    t = t_axis(seconds)
    e = np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-curve * np.maximum(0.0, t - attack) / max(decay, 1e-4))
    return e


def sweep(f0, f1, seconds, shape=3.0):
    t = t_axis(seconds)
    f = f1 + (f0 - f1) * np.exp(-shape * t / seconds)
    return np.sin(2 * np.pi * np.cumsum(f) / SR)


def tail(x, seconds, cutoff=1800.0, amount=0.35):
    """Cheap outdoor slap/reverb: decaying filtered noise convolved via FFT."""
    ir = lowpass(noise(seconds), cutoff) * np.exp(-5.0 * t_axis(seconds) / seconds)
    ir[:int(0.02 * SR)] *= np.linspace(0, 1, int(0.02 * SR))
    ir /= np.max(np.abs(ir)) + 1e-9
    n = len(x) + len(ir)
    wet = np.fft.irfft(np.fft.rfft(x, n) * np.fft.rfft(ir, n), n)
    wet /= np.max(np.abs(wet)) + 1e-9
    out = np.zeros(n)
    out[:len(x)] += x
    return out + amount * wet * np.max(np.abs(x))


def pad(x, seconds):
    out = np.zeros(int(SR * seconds))
    out[:min(len(x), len(out))] = x[:len(out)]
    return out


def norm(x, peak=0.9):
    return x / (np.max(np.abs(x)) + 1e-9) * peak


def save(name, x, peak=0.9):
    os.makedirs(OUT, exist_ok=True)
    data = (np.clip(norm(x, peak), -1, 1) * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())
    print("wrote", name, "%.2fs" % (len(x) / SR))


def gunshot(length=0.6, body=110.0, crack=5000.0, crack_amt=1.0, body_amt=1.0, tail_s=0.9, tail_amt=0.45):
    d = 0.35
    crack_n = highpass(noise(d), crack * 0.4) * env(d, 0.0005, 0.03, 6)
    mid = lowpass(noise(d), crack) * env(d, 0.001, 0.07, 5)
    thump = sweep(body * 2.2, body, d, 6) * env(d, 0.001, 0.12, 4)
    x = crack_amt * crack_n * 0.8 + mid + body_amt * thump * 1.4
    return pad(tail(x, tail_s, 2200, tail_amt), length + tail_s)


def explosion(seconds=3.5, low=45.0, crack=0.6, rumble_cut=380.0):
    t = t_axis(seconds)
    blast = lowpass(noise(seconds), 2400) * env(seconds, 0.002, 0.25, 5) * crack
    rumble = lowpass(noise(seconds), rumble_cut) * env(seconds, 0.01, seconds * 0.55, 3)
    boom = sweep(low * 3, low, seconds, 5) * env(seconds, 0.003, 0.9, 3)
    debris = highpass(noise(seconds), 2500) * env(seconds, 0.3, seconds * 0.4, 3) * (rng.random(len(t)) > 0.985) * 3
    return tail(blast + rumble * 2.2 + boom * 1.8 + debris * 0.4, 1.5, 900, 0.3)


def main():
    # Weapons (player and enemy)
    save("SW_Rifle", gunshot(0.4, 120, 5200))
    save("SW_SMG", gunshot(0.25, 150, 6000, body_amt=0.7, tail_s=0.6, tail_amt=0.35))
    save("SW_Shotgun", gunshot(0.7, 80, 3500, body_amt=1.6, tail_s=1.2, tail_amt=0.55))
    save("SW_DMR", gunshot(0.6, 95, 7000, crack_amt=1.6, body_amt=1.2, tail_s=1.4, tail_amt=0.6))
    pulse_d = 0.35
    zap = sweep(2400, 380, pulse_d, 4) * env(pulse_d, 0.001, 0.1, 4) + 0.4 * sweep(4800, 900, pulse_d, 5) * env(pulse_d, 0.0, 0.05, 5)
    save("SW_Pulse", tail(zap + 0.3 * highpass(noise(pulse_d), 3000) * env(pulse_d, 0, 0.02, 6), 0.5, 3000, 0.3))
    save("SW_Rotary", gunshot(0.18, 70, 2800, crack_amt=0.6, body_amt=1.5, tail_s=0.4, tail_amt=0.3))
    save("SW_Cannon30", gunshot(0.6, 55, 2600, crack_amt=0.8, body_amt=2.2, tail_s=1.4, tail_amt=0.6))
    save("SW_TankGun", explosion(2.2, 38, 1.2, 600))

    # Explosions and impacts
    save("SW_Explosion", explosion(3.5))
    save("SW_ExplosionSmall", explosion(1.6, 70, 0.9, 700))
    orbital = explosion(6.0, 30, 1.0, 260)
    whine = sweep(3200, 600, 1.2, 1.5) * env(1.2, 0.8, 0.2, 2) * 0.25
    save("SW_OrbitalImpact", np.concatenate([whine, orbital]))
    imp_d = 0.12
    save("SW_Impact", highpass(noise(imp_d), 1500) * env(imp_d, 0.0005, 0.02, 6) + 0.5 * sweep(900, 300, imp_d, 6) * env(imp_d, 0, 0.03, 5))

    # Call-ins
    boom_d = 2.5
    sonic = lowpass(noise(boom_d), 500) * env(boom_d, 0.001, 0.4, 3) + sweep(120, 40, boom_d, 4) * env(boom_d, 0.002, 0.5, 3)
    save("SW_PodLanding", tail(sonic, 1.0, 700, 0.3))
    beep_d = 0.18
    save("SW_PlantBeep", np.sin(2 * np.pi * 1760 * t_axis(beep_d)) * env(beep_d, 0.002, 0.12, 4))
    alarm_d = 1.6
    tt = t_axis(alarm_d)
    save("SW_Alarm", np.sign(np.sin(2 * np.pi * (520 + 180 * (np.sin(2 * np.pi * 2.5 * tt) > 0)) * tt)) * 0.4 * env(alarm_d, 0.01, alarm_d, 0.8))

    # Radio and UI
    sq_d = 0.22
    squelch = highpass(noise(sq_d), 1800) * env(sq_d, 0.001, 0.08, 4) * 0.6
    chirp = np.sin(2 * np.pi * 2200 * t_axis(sq_d)) * (t_axis(sq_d) < 0.06) * 0.5
    save("SW_RadioSquelch", lowpass(squelch + chirp, 4500), 0.6)
    hm_d = 0.08
    save("SW_HitMarker", np.sin(2 * np.pi * 2600 * t_axis(hm_d)) * env(hm_d, 0.0005, 0.03, 6) + 0.3 * highpass(noise(hm_d), 4000) * env(hm_d, 0, 0.01, 6), 0.5)
    k_d = 0.35
    save("SW_KillConfirm", (np.sin(2 * np.pi * 1320 * t_axis(k_d)) + 0.6 * np.sin(2 * np.pi * 1980 * t_axis(k_d))) * env(k_d, 0.001, 0.2, 4), 0.5)
    ui_d = 0.1
    save("SW_UIClick", np.sin(2 * np.pi * 900 * t_axis(ui_d)) * env(ui_d, 0.0005, 0.04, 6), 0.4)

    # Loops (seamless: crossfade the end into the start)
    def loop(x, fade=1.0):
        n = int(fade * SR)
        out = x[:-n].copy()
        ramp = np.linspace(0, 1, n)
        out[:n] = out[:n] * ramp + x[-n:] * (1 - ramp)
        return out

    wd = 14.0
    tt = t_axis(wd)
    gust = 0.55 + 0.45 * np.sin(2 * np.pi * tt / 7.0) * np.sin(2 * np.pi * tt / 3.1 + 1.0)
    wind = lowpass(noise(wd), 650) * gust + 0.25 * highpass(lowpass(noise(wd), 2500), 900) * gust ** 2
    save("SW_AmbWind", loop(wind), 0.5)
    hd = 12.0
    tt = t_axis(hd)
    hum = 0.6 * np.sin(2 * np.pi * 55 * tt) + 0.3 * np.sin(2 * np.pi * 110 * tt) + 0.15 * np.sin(2 * np.pi * 165.5 * tt)
    vents = lowpass(noise(hd), 400) * 0.8
    clank = np.zeros(len(tt))
    for _ in range(6):
        p = rng.integers(0, len(tt) - SR)
        c = sweep(400, 180, 0.6, 5) * env(0.6, 0.001, 0.3, 5) * 0.4
        clank[p:p + len(c)] += c
    save("SW_AmbHangar", loop(hum + vents + tail(clank, 1.5, 1200, 0.6)[:len(tt)]), 0.45)
    ed = 6.0
    tt = t_axis(ed)
    engine = sum(np.sin(2 * np.pi * f * tt) / (k + 1) for k, f in enumerate((48, 96, 144.5, 192, 289)))
    engine = engine * (0.8 + 0.2 * np.sin(2 * np.pi * 9 * tt)) + 0.5 * lowpass(noise(ed), 300)
    save("SW_EngineGround", loop(engine, 0.5), 0.5)
    tt = t_axis(ed)
    turbine = 0.4 * np.sin(2 * np.pi * (1800 + 30 * np.sin(2 * np.pi * 0.3 * tt)) * tt) + lowpass(noise(ed), 900) * 1.2 + 0.5 * np.sin(2 * np.pi * 70 * tt)
    save("SW_EngineGunship", loop(turbine, 0.5), 0.5)


if __name__ == "__main__":
    main()

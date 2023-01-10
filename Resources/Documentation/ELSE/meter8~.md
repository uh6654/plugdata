---
title: meter8~

description: Octaaphonic VU-meter

categories:
- object

pdcategory: GUI

arguments:
- type: float
  description: window size for the [vu~] objects
  default:
- type: float
  description: hop size for the [vu~] objects
  default:

inlets:
  nth:
  - type: signal
    description: incoming signal channels to be vu-metered

outlets:
  nth:
  - type: signal
    description: incoming signals are passed through
  2nd:
  - type: list
    description: vu values (RMS/peak amplitude in dBFS) of inputs

draft: false
---

[meter4~] is a convenient qudraphonic VU-meter abstraction based on [vu~] and vanilla's [vu] GUI - see also: [meter~], [meter2~] and [meter4~].
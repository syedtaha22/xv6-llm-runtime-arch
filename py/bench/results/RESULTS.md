Mean +- standard deviation over 5 runs per cell.

### T1 (prompt 5 tokens, 46 sampled tokens)

Forward passes per second

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 6.9 +- 0.3 | 17.2 +- 0.6 | 16.2 +- 0.2 | 20.0 +- 0.3 |
| rr | 7.6 +- 0.1 | 12.2 +- 0.3 | 11.5 +- 0.1 | 10.9 +- 0.0 |

Speedup over one thread

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 1.00x | 2.49x | 2.35x | 2.91x |
| rr | 1.00x | 1.60x | 1.50x | 1.42x |

Time to first token (ms)

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 852 +- 42 | 369 +- 14 | 403 +- 19 | 370 +- 5 |
| rr | 652 +- 31 | 399 +- 18 | 422 +- 3 | 451 +- 7 |

### T2 (prompt 5 tokens, 96 sampled tokens)

Forward passes per second

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 7.1 +- 0.1 | 18.2 +- 0.2 | 16.1 +- 0.2 | 19.9 +- 0.3 |
| rr | 7.5 +- 0.0 | 12.0 +- 0.2 | 11.2 +- 0.0 | 10.7 +- 0.0 |

Speedup over one thread

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 1.00x | 2.57x | 2.28x | 2.81x |
| rr | 1.00x | 1.60x | 1.50x | 1.43x |

Time to first token (ms)

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 843 +- 21 | 349 +- 11 | 408 +- 18 | 380 +- 4 |
| rr | 642 +- 14 | 400 +- 14 | 433 +- 8 | 450 +- 3 |

### T3 (prompt 50 tokens, 1 sampled tokens)

Forward passes per second

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 6.8 +- 0.2 | 18.3 +- 0.4 | 16.3 +- 0.3 | 20.3 +- 0.3 |
| rr | 7.6 +- 0.1 | 12.1 +- 0.2 | 11.4 +- 0.0 | 10.8 +- 0.0 |

Speedup over one thread

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 1.00x | 2.69x | 2.40x | 2.99x |
| rr | 1.00x | 1.60x | 1.51x | 1.43x |

Time to first token (ms)

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 7472 +- 232 | 2797 +- 49 | 3148 +- 43 | 2572 +- 31 |
| rr | 6606 +- 45 | 4129 +- 59 | 4386 +- 14 | 4609 +- 8 |

### T4 (prompt 50 tokens, 51 sampled tokens)

Forward passes per second

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 7.0 +- 0.2 | 17.9 +- 0.6 | 16.3 +- 0.2 | 20.1 +- 0.2 |
| rr | 7.4 +- 0.1 | 12.0 +- 0.1 | 11.2 +- 0.1 | 10.7 +- 0.0 |

Speedup over one thread

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 1.00x | 2.58x | 2.34x | 2.89x |
| rr | 1.00x | 1.62x | 1.51x | 1.44x |

Time to first token (ms)

| System | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| linux | 7353 +- 179 | 2866 +- 98 | 3160 +- 53 | 2612 +- 31 |
| rr | 6664 +- 107 | 4097 +- 54 | 4428 +- 27 | 4619 +- 26 |

### xv6 (rr) rate as a fraction of Linux rate

| Test | 1 threads | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| T1 | 1.11 | 0.71 | 0.71 | 0.54 |
| T2 | 1.06 | 0.66 | 0.70 | 0.54 |
| T3 | 1.11 | 0.66 | 0.70 | 0.53 |
| T4 | 1.07 | 0.67 | 0.69 | 0.53 |


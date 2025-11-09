# generate_math_test.py
import math
import random
import struct

def float_to_hex(f):
    """Convert float to hex representation for precise storage"""
    return hex(struct.unpack('<I', struct.pack('<f', f))[0])

def generate_sqrtf_cases():
    """Generate sqrtf test cases"""
    cases = []
    
    # Edge cases
    cases.extend([
        (0.0, 0.0),           # sqrt(0)
        (1.0, 1.0),           # sqrt(1)
        (4.0, 2.0),           # sqrt(4)
        (9.0, 3.0),           # sqrt(9)
        (16.0, 4.0),          # sqrt(16)
        (-1.0, float('nan')), # sqrt(-1) = NaN
        (-4.0, float('nan')), # sqrt(-4) = NaN
    ])
    
    # Perfect squares
    for i in range(5, 20):
        x = i * i
        cases.append((x, float(i)))
    
    # Small positive numbers
    for i in range(20):
        x = random.uniform(0.0001, 0.1)
        cases.append((x, math.sqrt(x)))
    
    # Large numbers
    for i in range(20):
        x = random.uniform(100.0, 10000.0)
        cases.append((x, math.sqrt(x)))
    
    # Very small numbers
    for i in range(20):
        x = random.uniform(1e-10, 1e-5)
        cases.append((x, math.sqrt(x)))
    
    # Random normal cases
    for i in range(30):
        x = random.uniform(0.1, 100.0)
        cases.append((x, math.sqrt(x)))
    
    return cases[:100]  # Limit to 100 cases

def generate_expf_cases():
    """Generate expf test cases"""
    cases = []
    
    # Edge cases
    cases.extend([
        (0.0, 1.0),                    # exp(0)
        (1.0, math.exp(1)),            # exp(1)
        (-1.0, math.exp(-1)),          # exp(-1)
        (88.0, float('inf')),          # overflow
        (-88.0, 0.0),                  # underflow
        (100.0, float('inf')),         # large positive
        (-100.0, 0.0),                 # large negative
    ])
    
    # Positive numbers
    for i in range(20):
        x = random.uniform(0.1, 5.0)
        cases.append((x, math.exp(x)))
    
    # Negative numbers
    for i in range(20):
        x = random.uniform(-5.0, -0.1)
        cases.append((x, math.exp(x)))
    
    # Near overflow
    for i in range(10):
        x = random.uniform(50.0, 88.0)
        cases.append((x, math.exp(x)))
    
    # Near underflow
    for i in range(10):
        x = random.uniform(-88.0, -50.0)
        cases.append((x, math.exp(x)))
    
    # Small values
    for i in range(20):
        x = random.uniform(-2.0, 2.0)
        cases.append((x, math.exp(x)))
    
    # Random cases
    for i in range(33):
        x = random.uniform(-10.0, 10.0)
        cases.append((x, math.exp(x)))
    
    return cases[:100]

def generate_powf_cases():
    """Generate powf test cases"""
    cases = []
    
    # Edge cases
    cases.extend([
        (0.0, 0.0, 1.0),           # 0^0 = 1
        (0.0, 1.0, 0.0),           # 0^1 = 0
        (0.0, 2.0, 0.0),           # 0^2 = 0
        (0.0, -1.0, float('inf')), # 0^-1 = inf
        (1.0, 0.0, 1.0),           # 1^0 = 1
        (1.0, 5.0, 1.0),           # 1^5 = 1
        (2.0, 0.0, 1.0),           # 2^0 = 1
        (2.0, 1.0, 2.0),           # 2^1 = 2
        (2.0, 2.0, 4.0),           # 2^2 = 4
        (2.0, -1.0, 0.5),          # 2^-1 = 0.5
    ])
    
    # Integer exponents
    for base in [0.5, 1.5, 2.5, 3.0, 4.0]:
        for exp in range(-5, 6):
            if base == 0.0 and exp <= 0:
                continue
            cases.append((base, float(exp), math.pow(base, exp)))
    
    # Fractional exponents
    for base in [0.5, 2.0, 3.0, 4.0, 10.0]:
        for exp in [0.5, 0.25, 1.5, 2.5, -0.5, -1.5]:
            cases.append((base, exp, math.pow(base, exp)))
    
    # Negative bases with integer exponents
    for base in [-2.0, -3.0, -4.0]:
        for exp in [2.0, 3.0, 4.0]:  # Even exponents give positive results
            cases.append((base, exp, math.pow(base, exp)))
        for exp in [1.0, 3.0, 5.0]:  # Odd exponents give negative results
            cases.append((base, exp, math.pow(base, exp)))
    
    # Random cases with positive bases
    for i in range(40):
        base = random.uniform(0.1, 10.0)
        exponent = random.uniform(-3.0, 3.0)
        cases.append((base, exponent, math.pow(base, exponent)))
    
    # Special fractional cases
    for base in [0.1, 0.5, 2.0, 5.0, 10.0]:
        for exp in [0.1, 0.2, 0.333, 0.5, 0.75]:
            cases.append((base, exp, math.pow(base, exp)))
            cases.append((base, -exp, math.pow(base, -exp)))
    
    return cases[:100]

def generate_sinf_cases():
    """Generate sinf test cases"""
    cases = []
    pi = math.pi
    
    # Special angles
    special_angles = [
        (0.0, 0.0),
        (pi/6, 0.5),
        (pi/4, math.sqrt(2)/2),
        (pi/3, math.sqrt(3)/2),
        (pi/2, 1.0),
        (pi, 0.0),
        (2*pi, 0.0),
        (3*pi/2, -1.0),
    ]
    cases.extend(special_angles)
    
    # Negative angles
    for angle, expected in special_angles[1:]:  # skip 0
        cases.append((-angle, -expected))
    
    # Large multiples of pi
    for multiplier in [10, 50, 100, 1000]:
        angle = multiplier * pi
        cases.append((angle, math.sin(angle)))
        cases.append((-angle, math.sin(-angle)))
    
    # Random angles in [-2π, 2π]
    for i in range(30):
        angle = random.uniform(-2*pi, 2*pi)
        cases.append((angle, math.sin(angle)))
    
    # Angles in different quadrants
    for quadrant in range(4):
        for i in range(10):
            angle = random.uniform(quadrant * pi/2, (quadrant + 1) * pi/2)
            cases.append((angle, math.sin(angle)))
            cases.append((-angle, math.sin(-angle)))
    
    # Very small angles
    for i in range(10):
        angle = random.uniform(-0.001, 0.001)
        cases.append((angle, math.sin(angle)))
    
    # Large angles (periodicity test)
    for i in range(10):
        angle = random.uniform(1000.0, 10000.0)
        cases.append((angle, math.sin(angle)))
    
    return cases[:100]

def generate_cosf_cases():
    """Generate cosf test cases"""
    cases = []
    pi = math.pi
    
    # Special angles
    special_angles = [
        (0.0, 1.0),
        (pi/6, math.sqrt(3)/2),
        (pi/4, math.sqrt(2)/2),
        (pi/3, 0.5),
        (pi/2, 0.0),
        (pi, -1.0),
        (2*pi, 1.0),
        (3*pi/2, 0.0),
    ]
    cases.extend(special_angles)
    
    # Negative angles (cos is even function)
    for angle, expected in special_angles[1:]:  # skip 0
        cases.append((-angle, expected))
    
    # Large multiples of pi
    for multiplier in [10, 50, 100, 1000]:
        angle = multiplier * pi
        cases.append((angle, math.cos(angle)))
        cases.append((-angle, math.cos(-angle)))
    
    # Random angles in [-2π, 2π]
    for i in range(30):
        angle = random.uniform(-2*pi, 2*pi)
        cases.append((angle, math.cos(angle)))
    
    # Angles in different quadrants
    for quadrant in range(4):
        for i in range(10):
            angle = random.uniform(quadrant * pi/2, (quadrant + 1) * pi/2)
            cases.append((angle, math.cos(angle)))
            cases.append((-angle, math.cos(-angle)))
    
    # Very small angles
    for i in range(10):
        angle = random.uniform(-0.001, 0.001)
        cases.append((angle, math.cos(angle)))
    
    # Large angles (periodicity test)
    for i in range(10):
        angle = random.uniform(1000.0, 10000.0)
        cases.append((angle, math.cos(angle)))
    
    return cases[:100]

def write_test_cases():
    """Write all test cases to C header file"""
    test_cases = {
        'sqrtf': generate_sqrtf_cases(),
        'expf': generate_expf_cases(),
        'powf': generate_powf_cases(),
        'sinf': generate_sinf_cases(),
        'cosf': generate_cosf_cases(),
    }
    
    with open('math_test_cases.h', 'w') as f:
        f.write('#ifndef MATH_TEST_CASES_H\n')
        f.write('#define MATH_TEST_CASES_H\n\n')
        
        # sqrtf test cases
        f.write('// sqrtf test cases - %d cases\n' % len(test_cases['sqrtf']))
        f.write('float sqrtf_inputs[] = {\n')
        for case in test_cases['sqrtf']:
            f.write('    %.10ff,\n' % case[0])
        f.write('};\n\n')
        
        f.write('float sqrtf_expected[] = {\n')
        for case in test_cases['sqrtf']:
            if math.isnan(case[1]):
                f.write('    0.0f / 0.0f, // NaN\n')
            elif math.isinf(case[1]):
                f.write('    1.0f / 0.0f, // INF\n')
            else:
                f.write('    %.10ff,\n' % case[1])
        f.write('};\n\n')
        
        f.write('const int sqrtf_count = %d;\n\n' % len(test_cases['sqrtf']))
        
        # expf test cases
        f.write('// expf test cases - %d cases\n' % len(test_cases['expf']))
        f.write('float expf_inputs[] = {\n')
        for case in test_cases['expf']:
            f.write('    %.10ff,\n' % case[0])
        f.write('};\n\n')
        
        f.write('float expf_expected[] = {\n')
        for case in test_cases['expf']:
            if math.isnan(case[1]):
                f.write('    0.0f / 0.0f, // NaN\n')
            elif math.isinf(case[1]):
                f.write('    1.0f / 0.0f, // INF\n')
            else:
                f.write('    %.10ff,\n' % case[1])
        f.write('};\n\n')
        
        f.write('const int expf_count = %d;\n\n' % len(test_cases['expf']))
        
        # powf test cases (special handling for two inputs)
        f.write('// powf test cases - %d cases\n' % len(test_cases['powf']))
        f.write('float powf_inputs[] = {\n')
        for case in test_cases['powf']:
            f.write('    %.10ff, %.10ff,\n' % (case[0], case[1]))
        f.write('};\n\n')
        
        f.write('float powf_expected[] = {\n')
        for case in test_cases['powf']:
            if math.isnan(case[2]):
                f.write('    0.0f / 0.0f, // NaN\n')
            elif math.isinf(case[2]):
                f.write('    1.0f / 0.0f, // INF\n')
            else:
                f.write('    %.10ff,\n' % case[2])
        f.write('};\n\n')
        
        f.write('const int powf_count = %d;\n\n' % len(test_cases['powf']))
        
        # sinf test cases
        f.write('// sinf test cases - %d cases\n' % len(test_cases['sinf']))
        f.write('float sinf_inputs[] = {\n')
        for case in test_cases['sinf']:
            f.write('    %.10ff,\n' % case[0])
        f.write('};\n\n')
        
        f.write('float sinf_expected[] = {\n')
        for case in test_cases['sinf']:
            f.write('    %.10ff,\n' % case[1])
        f.write('};\n\n')
        
        f.write('const int sinf_count = %d;\n\n' % len(test_cases['sinf']))
        
        # cosf test cases
        f.write('// cosf test cases - %d cases\n' % len(test_cases['cosf']))
        f.write('float cosf_inputs[] = {\n')
        for case in test_cases['cosf']:
            f.write('    %.10ff,\n' % case[0])
        f.write('};\n\n')
        
        f.write('float cosf_expected[] = {\n')
        for case in test_cases['cosf']:
            f.write('    %.10ff,\n' % case[1])
        f.write('};\n\n')
        
        f.write('const int cosf_count = %d;\n\n' % len(test_cases['cosf']))
        
        f.write('#endif\n')
    
    print("Generated comprehensive test cases:")
    for func, cases in test_cases.items():
        print("  %s: %d test cases" % (func, len(cases)))
    
    # Print some statistics
    print("\nTest coverage summary:")
    print("sqrtf: 0, small positive, perfect squares, large numbers, very small numbers")
    print("expf: 0, positive, negative, large positive (overflow), large negative (underflow)")
    print("powf: integer exponents, fractional exponents, negative bases, 0^0, x^0, 1^x")
    print("sinf/cosf: 0, π/6, π/4, π/3, π/2, π, 2π, large multiples of π, negative angles")

if __name__ == '__main__':
    write_test_cases()
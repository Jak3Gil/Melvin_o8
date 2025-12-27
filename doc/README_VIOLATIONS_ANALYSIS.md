# README Violations Analysis

## Found Violations

### 1. MIN_EPSILON Hardcoded Constant (Line 661)
**Location**: `compute_adaptive_epsilon()` function
**Code**: `const float MIN_EPSILON = 1e-6f;`
**Issue**: Hardcoded minimum epsilon used as fallback when `value_range <= 0.0f`
**README Violation**: Principle 2 - "No fallbacks are allowed - when no data exists, the system either uses minimal available context (node's own properties) or returns a neutral value (0.0f) meaning 'no operation', never a hardcoded assumption."
**Fix**: Return `0.0f` (neutral) when `value_range <= 0.0f` instead of `MIN_EPSILON`

### 2. Hardcoded Ranges in fmaxf/fminf (Lines 690, 825, 844, 885)
**Locations**: 
- Line 690: `fmaxf(0.1f, fminf(0.9f, smoothing))` - smoothing factor range
- Line 825: `fmaxf(1, fminf(adaptive_steps, 16))` - exploration steps cap
- Line 844: `fmaxf(0.3f, fminf(smoothing, 0.7f))` - initial smoothing range  
- Line 885: `fmaxf(5, fminf(growth_trigger, 20))` - bucket growth range

**Issue**: Hardcoded min/max bounds used to clip computed values
**README Violation**: Principle 3 - "Stability parameters (epsilon, clipping bounds, smoothing factors) are computed from observed data distributions"
**Status**: NEEDS VERIFICATION - These may be safety bounds (acceptable) or decision thresholds (violation)
**Test**: Verify these ranges are safety bounds preventing extreme values, not decision thresholds

### 3. Hardcoded Return Values (Lines 697, 806, 853, 888)
**Locations**:
- Line 697: `return 3;` - minimum default for adaptive min samples
- Line 806: `return 0.1f;` - default moderate increase
- Line 853: `return 0.5f;` - neutral when only node weight available
- Line 888: `return 10;` - default bucket growth trigger

**Issue**: Hardcoded return values used when no data available
**README Violation**: Principle 2 - "No fallbacks are allowed"
**Status**: NEEDS VERIFICATION - Some may be minimal context (acceptable), others may be fallbacks (violation)
**Test**: Verify these use minimal context or should return 0.0f

### 4. Hardcoded Initial Values (Lines 62, 70, 95, 166, 252)
**Locations**:
- Line 62: `return 256;` - minimum hash size for empty graph
- Line 70: `power_of_2 = 256;` - start from minimum
- Line 95: `if (hash_size < 256) hash_size = 256;` - minimum hash size
- Line 166: `*bucket_capacity = 4;` - initial bucket capacity
- Line 252: `stats->histogram_bucket_count = 100;` - initial bucket count

**Issue**: Hardcoded initial values
**README Violation**: Principle 2 - "All limits adapt to data"
**Status**: NEEDS VERIFICATION - Initial values that adapt later may be acceptable
**Test**: Verify these values adapt as data grows

## Next Steps

1. Fix confirmed violation: MIN_EPSILON should return 0.0f instead of hardcoded value
2. Verify hardcoded ranges are safety bounds, not thresholds
3. Verify hardcoded returns use minimal context appropriately
4. Verify initial values adapt properly


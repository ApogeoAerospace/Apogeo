# Doxygen style guide (adopted)

Use this style for all public headers in `src/core/` and `src/api/`.

## Class template

```cpp
/**
 * @brief One-line short description.
 *
 * More detailed behavior summary at contract level.
 */
class Example {
public:
    /**
     * @brief Executes one simulation step.
     * @param dt Simulation time delta in seconds.
     * @return true on success, false otherwise.
     * @note Thread-safe.
     */
    bool step(double dt);
};
```

## Rules

1. Public API: always use Doxygen blocks.
2. Use tags consistently:
   - `@brief`
   - `@param`
   - `@return`
   - `@note`
   - `@warning`
3. Include units in parameter documentation.
4. Keep runtime/algorithm details in `.cpp`, not in header comments.
5. Standard project documentation language: English.

## Official reference

The official project guide is:

- [`docs/DOCUMENTATION_STYLE_GUIDE.md`](../DOCUMENTATION_STYLE_GUIDE.md)

---

Documentation status: **Current**

Back to: [`docs/README.md`](../README.md)

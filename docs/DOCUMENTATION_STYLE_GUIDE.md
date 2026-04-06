# Documentation Style Guide - MoLab

> Status: **Implemented (active official guide)**

This document defines the official standard for documentation inside the MoLab source code. The entire team should follow these conventions to keep consistency and enable automated documentation generation.

## Official Tool: Doxygen

MoLab uses **Doxygen** as its documentation tool. Doxygen reads special comments in the code and generates HTML, PDF, or LaTeX documentation automatically.

### Doxygen installation

**Windows:**
```bash
# Using winget
winget install doxygen

# Or download from: https://www.doxygen.nl/download.html
```

**Linux:**
```bash
sudo apt install doxygen doxygen-gui graphviz
```

**macOS:**
```bash
brew install doxygen graphviz
```

### Generate documentation

From the project root:
```bash
doxygen Doxyfile
```

Documentation will be generated at `docs/generated/html/index.html`.

---

## Comment format

### Official style: Javadoc with `/**`

Use Javadoc-style double-asterisk blocks for all documentation comments:

```cpp
/**
 * @brief One-line short description.
 *
 * More detailed description of the element. It can span
 * multiple lines and explain behavior in depth.
 */
```

### Language

- **Documentation comments**: English
- **Variable, class, and method names**: English (matching existing code)

---

## Class documentation

Every public class must have a documentation block before its declaration.

### Template

```cpp
/**
 * @class ClassName
 * @brief Short class description.
 *
 * Detailed description of the class responsibility,
 * its purpose in the system, and how it relates to other components.
 *
 * @author Author name (optional)
 * @date Creation date (optional)
 *
 * @see RelatedClass
 */
class ClassName {
    // ...
};
```

---

## Method and function documentation

### Template

```cpp
/**
 * @brief Short method description.
 *
 * Detailed behavior description, including
 * special cases and side effects.
 *
 * @param parameter_name Parameter description.
 * @param other_parameter Description of another parameter.
 *
 * @return Description of the return value.
 *
 * @throws ExceptionType When this exception is thrown.
 *
 * @pre Precondition that must be true before calling.
 * @post Postcondition guaranteed after execution.
 *
 * @note Additional important notes.
 * @warning Warnings about incorrect usage.
 */
```

---

## Doxygen quick reference commands

| Command | Usage |
|---------|-------|
| `@brief` | Short description (one line) |
| `@param` | Document a parameter |
| `@return` | Document the return value |
| `@throws` | Document exceptions |
| `@see` | Reference related elements |
| `@note` | Informational note |
| `@warning` | Important warning |
| `@deprecated` | Mark as deprecated |
| `@todo` | Pending tasks |
| `@bug` | Known bugs |

---

## Verification checklist

Before committing, verify:

- [ ] Do all public classes have `@brief` and description?
- [ ] Do all public methods include `@param` and `@return`?
- [ ] Do new files include an `@file` block?
- [ ] Is documentation written in English?
- [ ] Does `doxygen Doxyfile` run without relevant warnings?

---

## Historical reference document

The historical version is kept at:

- [Deprecated guide version](./deprecated/DOCUMENTATION_STYLE_GUIDE.md)

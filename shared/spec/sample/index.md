# Marklens sample

A manual smoke test exercising every rendering path.

## Text

**Bold**, *italic*, ~~struck~~, `inline code`, and a [relative link](OTHER.md)
plus an [external link](https://example.com).

## Image (relative, resolves against this folder)

![icon](design/icon.svg)

## Table

| Feature   | Works |
|-----------|:-----:|
| Tables    | ✅ |
| Task list | ✅ |

## Task list

- [x] Render Markdown
- [ ] Take over the world

## Code

```python
def greet(name: str) -> str:
    return f"hello, {name}"
```

## Mermaid

```mermaid
graph TD
    A[Open .md] --> B{Rendered?}
    B -->|yes| C[Read it]
    B -->|no| D[File a bug]
```

> A viewer, not an editor.

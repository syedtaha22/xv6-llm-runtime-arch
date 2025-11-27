# Contributing Guidelines

### Purpose

This document defines the required conventions for branching, commits, pull requests, and code style across this repository. Consistency and clarity are mandatory for all contributions.

---

## 1. Branching Strategy

This project uses **long-lived research and feature branches**. Each branch isolates a subsystem, experiment, or focused enhancement.

### Branch Structure

| Branch Type              | Purpose                                                 | Source | Destination | Notes                              |
| :----------------------- | :------------------------------------------------------ | :----- | :---------- | :--------------------------------- |
| **`main`**               | Stable, reviewed work. Represents the current baseline. | N/A    | N/A         | Protected. No direct commits.      |
| **`exp/<topic>`**        | Experimental or research work.                          | `main` | `main` (PR) | For exploratory or benchmark work. |
| **`feat/<description>`** | Adds or modifies functionality.                         | `main` | `main` (PR) | Each branch = one feature.         |
| **`fix/<description>`**  | Bug fixes or corrections.                               | `main` | `main` (PR) | Keep scope minimal.                |

### Workflow Rules

1. Never commit directly to `main`.
2. Always create a new branch before starting work:

   ```bash
   git checkout main
   git pull origin main
   git checkout -b exp/kernel-memory
   ```
3. Keep branches focused. One topic per branch.
4. Open a Pull Request when ready for review.

---

## 2. Commit Message Convention

Each commit represents one logical, testable change. Use this structure:

```
<type>: <short, imperative description>

[optional body]

[optional footer]
```

### Allowed Types

| Type           | Description                                                       |
| :------------- | :---------------------------------------------------------------- |
| **`feat`**     | Add or modify functionality.                                      |
| **`fix`**      | Correct a bug or issue.                                           |
| **`docs`**     | Documentation or comments only.                                   |
| **`style`**    | Code formatting or readability changes.                           |
| **`refactor`** | Code structure changes without altering behavior.                 |
| **`perf`**     | Performance or efficiency improvements.                           |
| **`test`**     | Add or modify tests.                                              |
| **`chore`**    | General maintenance or configuration updates.                     |
| **`research`** | Experimental commits related to testing hypotheses or benchmarks. |
| **`exp`**      | Prototype-level or exploratory commits.                           |

### Commit Rules

* The **subject line** must be under 72 characters.
* Use **imperative mood** (“Add,” “Fix,” “Update”).
* The **body** may explain *what* changed and *why*, when needed.
* Wrap **body lines at 72 characters**.
* Include references or related issues in the **footer**.
* Do not commit untested or incomplete code.
* Remove temporary, debug, or commented-out code before committing.
* Avoid vague subjects like “update files” or “changes done.”

**Examples:**

```
feat: add new process scheduler prototype

research: evaluate impact of thread pinning on inference latency

fix: correct memory leak in buffer allocation

docs: describe runtime profiling method
```

---

## 3. Pull Requests and Review

1. Push your branch and open a Pull Request targeting `main`.
2. All PRs must be reviewed by at least one other member.
3. The reviewer checks correctness, clarity, and test coverage.
4. Use **Squash and Merge** to maintain clean history.
5. Delete branches after merging.

---

## 4. Coding Standards

### 4.1 C Code

* **Naming:**

  * Functions, variables: `snake_case`
  * Constants/macros: `UPPER_CASE_WITH_UNDERSCORES`
  * Structs, typedefs: `PascalCase`
* **Comments:** Use Doxygen-style for public interfaces.
* **Structure:**

  * Each function should have a clear, single purpose.
  * Avoid global variables unless justified by design.
  * Return error codes instead of printing messages.

**Example:**

```c
/**
 * @brief Initializes the thread scheduler.
 * 
 * Must be called before any threads are created.
 */
void scheduler_init(void);
```

---

### 4.2 Python Code

* **Follow PEP 8** standards.
* **Indentation:** 4 spaces.
* **Naming:**

  * Functions, variables: `snake_case`
  * Classes: `PascalCase`
  * Constants: `UPPER_CASE_WITH_UNDERSCORES`
  * Private members: prefix with `_`
* **Formatting:** Use `black`.

```bash
sudo apt install black
black .
```

**Example:**

```python
def average(a: int, b: int) -> float:
    """
    Compute the average of two numbers.

    Parameters
    ----------
    a : int
        First integer.
    b : int
        Second integer.

    Returns
    -------
    float
        The average value.
    """
    return (a + b) / 2
```

---

## 5. Best Practices

| Area              | Do                                                        | Don’t                              |
| :---------------- | :-------------------------------------------------------- | :--------------------------------- |
| **Branching**     | Use clear, descriptive names like `exp/kernel-scheduler`. | Push directly to `main`.           |
| **Committing**    | Keep each commit focused and atomic.                      | Mix unrelated changes.             |
| **Messages**      | Follow the commit format exactly.                         | Use vague or incomplete summaries. |
| **Pull Requests** | Test and review before merging.                           | Merge untested or unreviewed code. |
| **Cleanup**       | Remove experimental leftovers before merge.               | Leave temporary or debug files.    |
| **Code**          | Format, document, and lint before commit.                 | Skip docstrings or comments.       |
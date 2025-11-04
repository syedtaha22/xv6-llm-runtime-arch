# Commit and Branching Guidelines

### Purpose

This document defines the required conventions for commits, branches, and pull requests in this repository. These rules ensure a consistent and reviewable history across all contributors.

---

## 1. Branching Strategy

This project uses **long-lived research branches**. Each branch focuses on a specific subsystem, experiment, or optimization area.

### Branch Structure

| Branch Type              | Purpose                                                 | Source | Destination     | Notes                              |
| :----------------------- | :------------------------------------------------------ | :----- | :-------------- | :--------------------------------- |
| **`main`**               | Stable, verified work. Represents the current baseline. | N/A    | N/A             | Protected. No direct commits.      |
| **`exp/<topic>`**        | Experimental or research work.                          | `main` | `main` (via PR) | Used for long-running experiments. |
| **`feat/<description>`** | Adds or updates functionality.                          | `main` | `main` (via PR) | Scoped to a single addition.       |
| **`fix/<description>`**  | Bug fixes or corrections.                               | `main` | `main` (via PR) | Keep targeted and minimal.         |

### Workflow Rules

1. Never commit directly to `main`.
2. Always create a new branch before starting work:

   ```bash
   git checkout main
   git pull origin main
   git checkout -b exp/kernel-memory
   ```
3. Keep changes focused. One topic per branch.
4. Submit a Pull Request when ready for review.

---

## 2. Commit Message Convention

All commits must follow the format below. Each commit should represent one logical, testable change.

### Format

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
* Use the **imperative mood** (“Add,” “Fix,” “Update”).
* The **body** may explain *what* changed and *why*, when needed.
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

1. Push your branch and open a Pull Request to `main`.
2. Each PR must be reviewed by another team member.
3. The reviewer ensures correctness, clarity, and functional stability.
4. Merge using **Squash and Merge** after approval.
5. Delete the branch after merging.

---

## 4. Best Practices

| Area              | Do                                                        | Don’t                              |
| :---------------- | :-------------------------------------------------------- | :--------------------------------- |
| **Branching**     | Use clear, descriptive names like `exp/kernel-scheduler`. | Push directly to `main`.           |
| **Committing**    | Keep each commit focused and atomic.                      | Mix unrelated changes.             |
| **Messages**      | Follow the commit format exactly.                         | Use vague or incomplete summaries. |
| **Pull Requests** | Test and review before merging.                           | Merge untested or unreviewed code. |
| **Cleanup**       | Remove experimental leftovers before merge.               | Leave temporary or debug files.    |
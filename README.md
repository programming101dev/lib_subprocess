# lib_subprocess

`p101_subprocess` owns shell-free child execution, descriptor plumbing,
bounded capture, and wait semantics. Callers retain authority over argv,
environment policy, and interpretation of child status.

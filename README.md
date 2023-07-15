# Minishell

This is a toy linux shell implemented as part of the course CSC33 - Operating Systems - and which accepts I/O redirection, pipelines, and stopping/continuing jobs through <kbd>CTRL</kbd>+<kbd>Z</kbd> and ``fg N``, where "N" is the 1-indexed index of the job (use ``jobs`` for a list of jobs with their indexes).

Like famous Linux shells, programs should be used with a relative or absolute path. However, in this minishell, usual built-in programs such as ``ls`` are not in any "path" so those commands should also be given as paths to their binaries (i.e., use ``/bin/ls`` instead of just ``ls``).

**Most of job control functionalities were implemented according to [GNU C Libray manual's "Implementing a Shell" chapter](https://www.gnu.org/software/libc/manual/html_node/Implementing-a-Shell.html)**, but that guide was not completed (see section [Missing Pieces](https://www.gnu.org/software/libc/manual/html_node/Missing-Pieces.html)). Basically, I implemented the ``main`` body and a few functions, but most functions were directly taken (eventually modified) from that manual. This toy shell is really a toy shell so no serious use is expected, but if someone decides to use it, be sure you are doing so in respect to the GNU C Libray manual's license (GFDL).


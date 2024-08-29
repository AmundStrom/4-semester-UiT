fs_test.py test different edge cases and stress tests most of the shell commands.
I recomend to not run all tests together, since it may be difficult to read the results of each test.
On line 167, you could increase the stress test of the depth of the file system, this requires expansion of the shell_sim's path buffer. As the comment mentions.
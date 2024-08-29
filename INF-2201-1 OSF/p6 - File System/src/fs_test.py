# This pre-code test file has been corrected by nlo014@uit.no spring semester 2022. 
# Improved readability of tests to predict what should happen
# File has been updated to run at python 3.8.10 or newer

# Difference from old 2.7.17 to 3.8.10 is that all strings have to
# be passed into subprocesses with a bit converter.
# subprocess.communicate also does not support [0] as argument anymore

import os, subprocess

# old imports that are not needed to run the test
# import sys, string, time

# Legacy variables that will simply be left as false
fsck_implemented = False
flush_implemented = False

# INSERT NAME OF SIMULATION EXECUTABLE HERE
executable = 'p6sh'

# legacy function that does not do anything in our case besides doing ls
def do_fsck() :
    if (fsck_implemented==True) :
        p.stdin.write(b'fsck\n')
        p.stdin.write(b'ls\n')
    else :
        p.stdin.write(b'ls\n')
    p.stdin.flush()

# seems like a legacy function that no longer has much of a use by default
# it seems to verify that a given input command will generate a given output
def check(input, output) :
    p.stdin.write(b'%s\n' %(input))
    p.stdin.flush()
    p.stdout.flush()
    out = p.stdout.read()
    print(out)
    if out != output :
        print ("File content does not match, command", input)
    else:
        print ("ok\n")

# exits the file system        
def do_exit():
    p.stdin.write(b'exit\n')
    return p.communicate()

# test1: uses most of the commands,
#        should not generate any errors
def test1():
    print('THIS IS A FIXED VERSION OF THE PRE-CODE TEST FILE\nCORRECTIONS AND MODIFICATIONS HAVE BEEN DONE BY nlo014@uit.no SPRING 2022 SEMESTER\n')
    print ("----------Starting Test1----------\n")
    print("testing that most commands work\n")
    do_fsck()
    p.stdin.write(b'cat foo\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'stat foo\n')
    p.stdin.write(b'mkdir d1\n')
    p.stdin.write(b'stat d1\n')
    p.stdin.write(b'cd d1\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'cat foo\n')
    p.stdin.write(b'ABCD\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'ln foo bar\n')
    do_fsck()
    p.stdin.write(b'ls\n')
    p.stdin.write(b'cat bar\n')
    p.stdin.write(b'.\n')
    do_exit()
    print("\n\nno errors should be generated\n")
    print ("\n----------Test1 Finished----------\n")

# test2: open/write/close,
#        should not give any errors
def test2() :
    print ("----------Starting Test2----------\n")
    print("testing open/write/close\n")
    do_fsck()
    p.stdin.write(b'cat testf\n')
    p.stdin.write(b'WELL\n')
    p.stdin.write(b'DONE\n')
    p.stdin.write(b'.\n')
    do_exit()
    print("\n\nno errors should be generated\n")
    print ("\n----------Test2 Finished----------\n")

# test3: open with readonly test,
#        should not give any errors
def test3():
    print ("----------Starting Test3----------\n")
    print("testing open with readonly\n")
    do_fsck()
    p.stdin.write(b'more testf\n')
    do_exit()
    print("\n\nno errors should be generated\n")
    print ("\n----------Test3 Finished----------\n")

# test4: rm tests,
#       should only give error on cat command
def test4():
    print ("----------Starting Test4----------\n")
    print("testing all remove commands\n")
    do_fsck()
    p.stdin.write(b'cd d1\n')
    p.stdin.write(b'rm foo\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'more bar\n')  # ABCD
    p.stdin.write(b'rm bar\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'more bar\n') # cat should fail
    do_exit()
    print("\n\nshould not be able to open bar\n")
    print ("\n----------Test4 Finished----------\n")


# test5: simple error cases,
#        all commands should get an error
def test5() :
    print ("----------Starting Test5----------\n")
    print("generating errors with rmdir, cd, ln and rm\n")
    do_fsck()
    p.stdin.write(b'rmdir non_exitsting\n') # Problem with removing directory
    p.stdin.write(b'cd non_exitsing\n') # Problem with changing directory
    p.stdin.write(b'ln non_existing non_existing_too\n') # Problem with ln
    p.stdin.write(b'rm non_existing\n') # Problem with rm
    do_exit()
    print("\n\ntested removing invalid directory\n")
    print("tested changng to invalid directory\n")
    print("tested linking to invalid directory\n")
    print("tested removing invalid hardlink\n")
    print ("\n----------Test5 Finished----------\n")

# test 6: create lots of files in one dir, 
#         should not generate any errors,
#         requires implementation of directories using multiple blocks for dirents
#         requires the use of multiple blocks to hold disk inodes
def test6():
    print ("----------Starting Test6----------\n")
    print("stress testing by generating many files in same directory\n")
    p.stdin.write(b'mkdir d00\n')
    p.stdin.write(b'cd d00\n')
    # num_files by default is 45
    num_files = 45
    for i in range(1, num_files):
        p.stdin.write(b'cat f%d\n' % i)
        p.stdin.write(b'ABCDE\n')
        p.stdin.write(b'.\n')
    p.stdin.write(b'stat f3\n')  # 6
    p.stdin.write(b'more f1\n')  # ABCDE
    p.stdin.write(b'more f33\n')   # ABCDE
    do_exit()
    print("it should print ABCDE twice\n")
    print ("\n----------Test6 Finished----------\n")


# test 7: create a lot of directories,
#         requires expansion of the shell_sim's path buffer
def test7() :
    print ("----------Starting Test7----------\n")
    print("stress testing directory depth on file system\n")
    p.stdin.write(b'ls\n')
    p.stdin.write(b'mkdir d2\n')
    p.stdin.write(b'cd d2\n')
    p.stdin.write(b'ls\n')
    # idx is by default 50
    idx = 13
    for i in range (1, idx) :
        p.stdin.write(b'mkdir d%d\n' %i)
        p.stdin.write(b'stat d%d\n' %i)   #DIRECTORY
        p.stdin.write(b'cd d%d\n' %i)
    # by default steps_back is 4
    steps_back = 4
    for j in range (1, steps_back) :
        p.stdin.write(b'cd ..\n')
    idx = idx - steps_back
    p.stdin.write(b'stat .\n') # DIRECTORY
    p.stdin.write(b'ls\n')
    do_exit()
    print('\n\nThere should be no errors\n')
    print ("\n----------Test7 Finished----------\n")

def test8() :
    print("----------Starting Test8----------\n")
    print("Edge cases for mkdir\n")

    p.stdin.write(b'mkdir dir\n')
    p.stdin.write(b'mkdir Dir\n')
    p.stdin.write(b'mkdir Dir1!#%&\n')

    p.stdin.write(b'mkdir Dir\n')     # mkdir with existing name
    p.stdin.write(b'mkdir dir\n')     # mkdir with existing name
    p.stdin.write(b'mkdir .\n')       # mkdir with existing name
    p.stdin.write(b'mkdir ..\n')      # mkdir with existing name
    p.stdin.write(b'mkdir /dir\n')    # Cannot create dir with "/" in name

    p.stdin.write(b'ls\n')

    p.stdin.write(b'cd dir\n')        # Change direcotry

    p.stdin.write(b'mkdir dir\n')
    p.stdin.write(b'mkdir Dir\n')
    p.stdin.write(b'mkdir Dir1!#%&\n')

    p.stdin.write(b'ls\n')

    do_exit()
    print('\n\nThere should be 5 errors\n')
    print ("\n----------Test8 Finished----------\n")

def test9() :
    print("----------Starting Test9----------\n")
    print("Edge cases for cd\n")

    # Should still be in root
    p.stdin.write(b'cd .\n')
    p.stdin.write(b'cd ..\n')
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'cd /./.././../.././.\n')
    p.stdin.write(b'ls\n')

    p.stdin.write(b'cd /\n')    # Clean
    p.stdin.write(b'ls\n')

    p.stdin.write(b'cd dir\n')      # cd with non existing name, error
    p.stdin.write(b'cd /dir\n')     # cd with non existing name, error

    # cd into directory and file
    p.stdin.write(b'mkdir dir\n')
    p.stdin.write(b'cd dir\n')
    p.stdin.write(b'cat text\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'cd text\n')     # cd into file, error
    p.stdin.write(b'ls\n')

    # Create path
    p.stdin.write(b'cd /\n')    # Clean
    p.stdin.write(b'mkdir path1\n')
    p.stdin.write(b'cd path1\n')
    p.stdin.write(b'mkdir path2\n')
    p.stdin.write(b'cd path2\n')
    p.stdin.write(b'mkdir path3\n')
    p.stdin.write(b'cd path3\n')

    # cd path
    p.stdin.write(b'cd /path1/path2/path3\n')   # absolute path
    p.stdin.write(b'ls\n')
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'cd path1/path2/path3\n')    # relative path
    p.stdin.write(b'ls\n')
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'cd path1/path2/pah3\n')     # cd into non existing path, error
    p.stdin.write(b'ls\n')

    do_exit()
    print('\n\nThere should be 4 errors\n')
    print ("\n----------Test9 Finished----------\n")

def test10() :
    print("----------Starting Test10----------\n")
    print("Edge cases for rmdir\n")

    p.stdin.write(b'mkdir dir\n')   # create directory
    p.stdin.write(b'cat text\n')    # create file
    p.stdin.write(b'.\n')
    p.stdin.write(b'ls\n')

    p.stdin.write(b'rmdir text\n')  # remove file, error
    p.stdin.write(b'rmdir dir\n')
    p.stdin.write(b'rmdir dir2\n')  # remove non existing, error
    p.stdin.write(b'rmdir .\n')     # remove not allowed, error
    p.stdin.write(b'rmdir ..\n')    # remove not allowed, error
    p.stdin.write(b'rmdir /dir\n')  # remove not allowed, error

    p.stdin.write(b'ls\n')

    p.stdin.write(b'mkdir dir\n')   # create directory, with same inode as previous removed directory
    p.stdin.write(b'ls\n')

    # remove direcotry with data
    p.stdin.write(b'cd dir\n')
    p.stdin.write(b'mkdir dir2\n')
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'stat dir\n')
    p.stdin.write(b'rmdir dir\n')   # remove direcotry with data, error

    # go into direcotry with data and clean, then remove directory
    p.stdin.write(b'cd dir\n')
    p.stdin.write(b'rmdir dir2\n')
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'rmdir dir\n')
    p.stdin.write(b'ls\n')

    do_exit()
    print('\n\nThere should be 6 errors\n')
    print ("\n----------Test10 Finished----------\n")

def test11() :
    print("----------Starting Test11----------\n")
    print("Edge cases for cat/more\n")

    p.stdin.write(b'cat text\n')    # create file
    p.stdin.write(b'ABCD\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'stat text\n')
    p.stdin.write(b'more text\n')

    p.stdin.write(b'cat text\n')    # continue to write
    p.stdin.write(b'EFGH\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'stat text\n')
    p.stdin.write(b'more text\n')

    p.stdin.write(b'mkdir dir\n')
    p.stdin.write(b'cat dir\n')     # not allowed to write direcotry, error
    p.stdin.write(b'more dir\n')    # not allowed to "more" direcotry, will not print error, but wont print any data

    p.stdin.write(b'cat .\n')       # invalid name, error
    p.stdin.write(b'cat ..\n')      # invalid name, error

    p.stdin.write(b'cat /text\n')   # allowed to open file with absoulute path
    p.stdin.write(b'.\n')

    p.stdin.write(b'ls\n')

    do_exit()
    print('\n\nThere should be 3 errors\n')
    print ("\n----------Test11 Finished----------\n")

def test12() :
    print("----------Starting Test12----------\n")
    print("Edge cases for ln/rm\n")

    p.stdin.write(b'cat text\n')    # create file
    p.stdin.write(b'ABCD\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'stat text\n')
    p.stdin.write(b'more text\n')

    p.stdin.write(b'ln text copy\n')    # create link
    p.stdin.write(b'stat copy\n')
    p.stdin.write(b'more copy\n')

    p.stdin.write(b'mkdir dir\n')
    p.stdin.write(b'ln dir dircopy\n')  # not allowed to link direcotry, error

    # Create link in another direcotry
    p.stdin.write(b'cd dir\n')
    p.stdin.write(b'ln /text copy2\n')  # create link
    p.stdin.write(b'stat copy2\n')
    p.stdin.write(b'more copy2\n')

    # REMOVE TEXT FILES
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'rm text\n')         # remove original text file
    p.stdin.write(b'stat copy\n')
    p.stdin.write(b'more copy\n')
    p.stdin.write(b'rm copy\n')         # remove copy
    p.stdin.write(b'ls\n')

    # remove copy in another direcotry
    p.stdin.write(b'cd dir\n')
    p.stdin.write(b'stat copy2\n')
    p.stdin.write(b'more copy2\n')
    p.stdin.write(b'rm copy2\n')        # remove copy

    p.stdin.write(b'cd /\n')
    p.stdin.write(b'rm dir\n')          # not allowed to remove direcotry, error


    do_exit()
    print('\n\nThere should be 2 errors\n')
    print ("\n----------Test12 Finished----------\n")

def test13() :
    print ("----------Starting Test13----------\n")
    print("stress testing write to a file\n")
    do_fsck()
    p.stdin.write(b'cat text\n')
    idx = 50
    for i in range (1, idx) :
        p.stdin.write(b'ABCDEFGHIJKLMNOPQRSTUVWXYZ\n')
    p.stdin.write(b'WELL\n')
    p.stdin.write(b'DONE\n')
    p.stdin.write(b'.\n')

    p.stdin.write(b'stat text\n')
    p.stdin.write(b'more text\n')
    
    do_exit()
    print("\n\nno errors should be generated\n")
    print ("\n----------Test13 Finished----------\n")

# launch a subprocess of the shell
def spawn_lnxsh():
    global p
    p = subprocess.Popen('./%s' %executable, shell=True, stdin=subprocess.PIPE)

# moves from test folder to src folder
os.chdir('..')
os.chdir('src')

# cleans object files, and compiles the simulation executable
os.system('make clean; make %s\n' %executable)

####### Main Test Program #######
print ("..........Starting..........\n\n")
spawn_lnxsh()
test1()
spawn_lnxsh()
test2()
spawn_lnxsh()
test3()
spawn_lnxsh()
test4()
spawn_lnxsh()
test5()
spawn_lnxsh()
test6()
spawn_lnxsh()
test7()
spawn_lnxsh()
test8()
# os.system('make clean; make %s\n' %executable)
# spawn_lnxsh()
# test9()
# os.system('make clean; make %s\n' %executable)
# spawn_lnxsh()
# test10()
# os.system('make clean; make %s\n' %executable)
# spawn_lnxsh()
# test11()
# os.system('make clean; make %s\n' %executable)
# spawn_lnxsh()
# test12()
# os.system('make clean; make %s\n' %executable)
# spawn_lnxsh()
# test13()
print ("\nFinished !")

# cleans object files
os.system('make clean\n')

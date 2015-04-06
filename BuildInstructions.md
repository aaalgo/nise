Building and installation instructions.

# Prerequisites #

NISE depends on the following software:

  * [Boost library](http://www.boost.org)
  * [Poco library](http://pocoproject.org/)
  * Hadoop C++ library (libhadooppipes.a & libhadooputils.a, manually install mapred/src/c++/{utils, pipes} in Hadoop source tree.)
  * [The CImg library](http://cimg.sourceforge.net/)
  * [LSHKIT](http://lshkit.sourceforge.net)
  * [Hadoop 0.21.0](http://archive.apache.org/dist/hadoop/core/hadoop-0.21.0/)
  * [jpeglib](http://www.ijg.org/files/jpegsrc.v8d.tar.gz) (we need the functions jpeg\_mem\_src/dest that only present in the 8.x version).
  * GCC with C++11 support, cmake, java, ant
  * Various other libraries typically present in the system.

# Building #

## Directory layout ##

Put the nise code, LSHKIT code and an empty build directory in a directory structure like the following (SRC can be any directory):

```
[SRC] $ ls
lshkit nise
```


## C++ part ##

> Run the following:

```
[SRC]$ cd nise
[nise]$ cmake -D CMAKE_BUILD_TYPE=release ../nise
[nise]$ make
```

The executables will be produced in the directory SRC/nise/bin .

## Java part ##

> Run the following

```
[SRC]$ cd nise/java
[java]$ ant jar
```

# Installation #

Build a directory in your hadoop file system and export that as $NISE\_HADOOP\_HOME.  Chose a installation directory in local file system and export that as $NISE\_HOME.

Run the following after successfully building the system:
```
[nise]$ ./setup.sh $NISE_HOME $NISE_HADOOP_HOME
```

# Running #

## Offline Indexing ##

Make sure the environment variables $NISE\_HOME and $NISE\_HADOOP\_HOME are correctly set.

Edit the script nise/script/example.sh and change the following variables IMAGE\_DIR and HADOOP\_WORK\_DIR (any temp dir in the hadoop file system to store the intermediate results) in the beginning of the script.

Change the current directory to where you want to index data structure to be stored and run the script.

## Starting the server ##

The above step should have produced a file "server.xml".  To start the server, simply run "$NISE\_HOME/bin/server" in the current directory.



```
```
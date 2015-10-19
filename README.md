# lkm_vfs
Linux Kernel Module adding an optional a file access mode with session semantic

This is a project assignement from the 2nd year Master Course ["Operating Systems II" (Italian)](http://www.dis.uniroma1.it/~quaglia/DIDATTICA/SO-II-6CRM/esame.html) at DIAG / Sapienza University of Rome.

The Operating Systems II course aims at presenting advanced design/implementation methods and techniques for modern operating systems. The topics dealt with by the course are bind to case studies oriented to LINUX systems and x86 compliant processors. The course requires basic knowledge on the structure and functionalities of operating systems, and knowledge on C programming.

The examination also requires the development of software sub-systems to be embedded within the LINUX kernel.

The specification for the sub-system implemented in this project are the following:

>Develop an extention of the LINUX VFS that adds the possibility to perform a sessione semantic access to a file accessible from the VFS (e.g. EXT2). 

>The file opening interface can either be implemented through an additional system call "opensession()" or reusing the existing system call "open()".

>When a file is opened in with session semantic, its content has to be copied in a different memory space than the BUFFER CACHE. Then, the updates will not be visible to any other opened channel on the file as long as the file has not been closed.

>Assume that the maximum size of the file handled by this subsystem is of 16KB), and that there exist a limit to the number of sessions opened in parallel.

This assignement had to be carried out in teams of two persons, and my teammate was Eleonora Calore.

Our implementation targets the 3.2.0-31 Linux Kernel.

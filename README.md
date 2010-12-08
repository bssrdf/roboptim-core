                                                                -*- outline -*-

For general information about the project, please refer to its homepage:
http://roboptim.sourceforge.net

* Where is the library documentation?

This README only covers configure/building issues, for more information
regarding this library usage, please refer to the Doxygen documentation.

If you have configured the package as explained in the first section, go
into you ``_build'' directory and type:

$ make doc

To view the HTML documentation: go in the ``doc/html'' directory and open the
``index.html'' file with your favorite internet browser.


* How to use Valgrind with the test suite?

All the tests launched by the test suite can be prefixed
with the environment variable ``CHECK_PREFIX''.

$ export CHECK_PREFIX='valgrind --log-file=valgrind.log'
$ make check


* Visual Studio Issues

** How to change default installation path (RobOptim can not be found)

Default installation path is ``C:/Libraries/roboptim''.
This can be changed in the .vsprops files placed in the folder
``msvc/roboptim-core''.

In the Visual Project window of the project, go to ``View > Property
Manager > roboptim-core > Debug > rob_opt''.

   NB: the same property file (rob_opt.vsprops) is opened by all the 3
   projects in Debug and Release mode.
   While ``opening roboptim-core > Debug > rob_opt'', you open the same
   file as ``simple > Release > rob_opt''.

Go to User Macros and modify the value of "ROB_OPTIM_INSTALL_DIR" to
the desired value.  This will change the folder of install of the
project and the include/lib folder used while compiling/linking your
project.

** I can't find Boost

Please open the file boost.vsprops (see above), and modify the include
files in ``C/C++ > General > Additional Include Directories''.
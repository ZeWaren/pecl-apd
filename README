PHP-APD
----------------------------------------------------------------------------- *

    Welcome to Version 2 of this README.  If you have read the README
    before, you may want to read it again.  There may be new stuff, 
    particularly regarding how to use the new pprof tracing.

Buid/Installation process:
----------------------------------------------------------------------------- *

    Make sure you have installed the CGI version of PHP and it is available
    in your current path along with the phpize script.

    Change into the source directory (either created from the downloaded TAR
    archive or from checking out CVS) an run the following commands:

        phpize  (not necessary if configure exists)
        ./configure
        make install


    This automatically should install the 'apd' zend module into your

        <PHP INSTALL PATH>/lib/php/<ZEND VERSION><-OPTIONAL_DEBUG>/

    directory. It isn't mandatory to have it there, in fact you can install
    it anywhere you care.


    In your INI file, add the following lines:

        zend_extension = /absolute/path/to/apd.so
        apd.dumpdir = /absolute/path/to/trace/directory
	apd.statement_trace = 0

    Depending on your PHP build, the zend_extension directive can be one
    of the following:

        zend_extension              (non ZTS, non debug build)
        zend_extension_ts           (    ZTS, non debug build)
        zend_extension_debug        (non ZTS,     debug build)
        zend_extension_debug_ts     (    ZTS,     debug build)

        zend_extension_debug = /absolute/path/to/apd.so

    apd.dumpdir:

        This can either be an absolute path or a relative path. Relative
        means always relative to your where from you run your executeable.

    apd.statement_trace:
        
        Whether or not to do per-line tracings.  There is a performance
        impact to enabling this.

    *** NOTE ******************************************************************
    *
    * If you're running the CGI version of PHP, you will need to add the '-e'
    * flag to enable extended information for apd to work properly:
    *
    *     php -e -f script.php
    *
    ***************************************************************************


Win32ism
----------------------------------------------------------------------------- *

    To build APD under windows you need a working PHP compilation
    environment as described on http://php.net/ (basically, it requires
    you to have MSVC, win32build.zip and bison/flex and some know how
    about how to get it to work). Also make sure that adp.dsp has DOS
    line endings! If it has unix line endings, MSVC will complain about it.

    You can use normal Windows path values for your PHP.INI settings:

        zend_extension_debug_ts = c:\phpdev\apd.dll
        apd.dumpdir = c:\phpdev\traces


How to use PHP-APD in your scripts with pprof (new style)
----------------------------------------------------------------------------- *

    This section describes the new way that profiling works under apd.  The 
    old method, which generates a non-machine parseable indented output is
    still available (see below), but is debing deprecated in favor of this,
    the pprof output style, which dumps a machine-readable file and provides
    a tool for interpreting the trace.  This allows for much greater specificity
    in the output, allows different sorts of summary generation, and a good
    deal of conrol over the call-tree output.  

    To set pprof tracing, just add the following line to the top of your
    phpp script:

        apd_set_pprof_trace();

    This will generate a pprof.<pid>.<ext> file in your apd.dumpdir.  The format
    of this file is as follows:

! 1 simple.php
& 1 main 2
+ 1 1 2
- 2
& 3 hello 2
+ 3 1 9
- 3
& 4 yell 2
+ 4 1 9
& 5 strtoupper 1
+ 5 1 4
- 5
- 4
- 1


! assigns a numeric index to a file so we can easily reference that file later. 
& declares a new function and it's index for the trace.  
+ shows a function being entered and the file index and line number it was called on. 
- shows a function being exited. 
@ shows a time elapsed in the form  
    @ file_id line_id process_user_clock_t process_system_clock_t process_real_clock_t

The syntax of the file is largely for informational purposes, it's not designed
for human consumption.  To extract human-readable information from the trace, use
the pprofp utility, bundled with APD.  The options to apd are:

pprofp <flags> <trace file>
    Sort options
    -a          Sort by alphabetic names of subroutines.
    -l          Sort by number of calls to subroutines
    -r          Sort by real time spent in subroutines.
    -R          Sort by real time spent in subroutines (inclusive of child calls).
    -s          Sort by system time spent in subroutines.
    -S          Sort by system time spent in subroutines (inclusive of child calls).
    -u          Sort by user time spent in subroutines.
    -U          Sort by user time spent in subroutines (inclusive of child calls).
    -v          Sort by average amount of time spent in subroutines.
    -z          Sort by user+system time spent in subroutines. (default)

    Display options
    -c          Display Real time elapsed alongside call tree.
    -m		Display file locations as well.
    -i          Suppress reporting for php builtin functions
    -O <cnt>    Specifies maximum number of subroutines to display. (default 15)
    -t          Display compressed call tree.
    -T          Display uncompressed call tree

Thus, to get basic output on the trace, we would do 

pprofp -u /tmp/pprof.<pid>.<ext>

and we get something like:

10:33:29(george@ballista)[~/pear/PECL/apd]> pprofp -u /tmp/pprof.23424.0

Trace for /data/storage/mirrors/www.php.net/index.php
Total Elapsed Time =    0.09
Total System Time  =    0.00
Total User Time    =    0.06


         Real         User        System             secs/    cumm
%Time (excl/cumm)  (excl/cumm)  (excl/cumm) Calls    call    s/call Name
------------------------------------------------------------------------
 33.3  0.02  0.02   0.02  0.02   0.00  0.00     7   0.0029    0.00 require_once
 33.3  0.01  0.01   0.02  0.02   0.00  0.00    55   0.0004    0.00 sprintf
 33.3  0.00  0.00   0.02  0.02   0.00  0.00   144   0.0001    0.00 feof
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 htmlspecialchars
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 substr
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 make_submit
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     7   0.0000    0.00 printf
  0.0  0.02  0.02   0.00  0.00   0.00  0.00     6   0.0000    0.00 printf
  0.0  0.02  0.02   0.00  0.00   0.00  0.00     6   0.0000    0.00 getimagesize
  0.0  0.00  0.01   0.00  0.02   0.00  0.00    17   0.0000    0.00 print_link
  0.0  0.00  0.00   0.00  0.00   0.00  0.00    10   0.0000    0.00 delim
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     7   0.0000    0.00 spacer
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     9   0.0000    0.00 hdelim
  0.0  0.00  0.02   0.00  0.01   0.00  0.00     2   0.0000    0.01 download_link
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 mirror_provider_url
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 mirror_provider
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 strftime
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 have_stats
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     1   0.0000    0.00 commonfooter
  0.0  0.00  0.00   0.00  0.00   0.00  0.00     2   0.0000    0.00 strrchr
  0.0  0.01  0.01   0.00  0.00   0.00  0.00     2   0.0000    0.00 filesize

This sorts output based on the usertime spent in subroutines, and ommits a call tree.  If
we added a -t

10:38:30(george@ballista)[~/pear/PECL/apd]> pprofp -u -t /tmp/pprof.23424.0

...

We would also get a call graph for the function.  Which is often very long.



KCachegrind compatible output
----------------------------------------------------------------------------- *
APD can convert it's pprof data files to a format readable by
kcachegrind.  kcachegrind is an amazing graphical profiler, available
here: http://kcachegrind.sourceforge.net/.

To perform the conversion, run

pprof2calltree -f /path/to/pprofp.<pid>.<ext>

This will generate a file in your local directory called

cachegrind.out.pprof.<pid>.<ext>

You can then load this file with kcachegrind as follows:

kcachegrind cachegrind.out.pprof.<pid>.<ext>

Implemented Debugging Functions
----------------------------------------------------------------------------- *
    override_function(string func_name, string func_args, string func_code):
        Syntax similar to create_function(). Overrides built-in functions
        (replaces them in the symbol table).

    rename_function(string orig_name, string new_name)
        Renames orig_name to new_name in the global function_table.  Useful
        for temporarly overriding builtin functions.

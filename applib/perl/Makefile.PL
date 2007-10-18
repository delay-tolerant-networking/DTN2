#
# GenPerlMakefile.pl
#
# Builds PerlMakefile.mk using MakeMaker
#

use ExtUtils::MakeMaker;

WriteMakefile(
	      'FIRST_MAKEFILE' => 'Makefile',
	      'NAME'   => 'dtnapi',
	      'INC'    => "-I $ENV{INCDIR}",
	      'LIBS'   => ["-L$ENV{LIBDIR} -ldtnapi"],
	      'OBJECT' => 'dtn_api_wrap_perl.o',
	      );

#
# GenPerlMakefile.pl
#
# Builds PerlMakefile.mk using MakeMaker
#

use ExtUtils::MakeMaker;

WriteMakefile(
	      'FIRST_MAKEFILE' => 'Makefile',
	      'NAME'   => 'dtnapi',
	      'LIBS'   => ["-ldtnapi"],
	      'OBJECT' => 'dtn_api_wrap_perl.o',
	      );



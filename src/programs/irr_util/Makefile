all:	../IRRd/templates.c ../irr_rpsl_check/attrs.rpsl ../../include/irr_rpsl_tokens.h

../IRRd/templates.c:	create_templates.pl rpsl.config
	@./create_templates.pl <rpsl.config >../IRRd/templates.c

../irr_rpsl_check/attrs.rpsl:	attr_tokens.pl rpsl.config
	@./attr_tokens.pl rpsl.config

../../include/irr_rpsl_tokens.h:	attr_tokens.pl rpsl.config
	@./attr_tokens.pl rpsl.config

clean:

distclean:
	rm -f ../IRRd/templates.c ../irr_rpsl_check/attrs.rpsl ../../include/irr_rpsl_tokens.h

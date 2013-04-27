include ../../Make.include

GOAL = rpsdist
RPSOBJS = rpsdist.o compression_utils.o file_utils.o \
	  init_utils.o journal.o mirror_update.o \
	  select.o update.o user_update.o \
	  utils.o
LIBINC = -L../irrd_ops/ -L../irr_rpsl_check/ \
	-L../pgp/ -L../hdr_comm/

LIBS = -lirrd_ops -lirrpgp -lhdrs $(SYS_LIBS)
INC = -I../../include/

all: $(GOAL)

$(GOAL): $(RPSOBJS)
	@gcc -Wall $(RPSOBJS) -g -o $@ $(LIBINC) $(LIBS) $(INC)

.c.o:
	@gcc -Wall $(INC) -g -c $< 

clean:
	$(RM) *.o core *.core *~*  $(GOAL) $(GOAL).purify

debug:
	@gcc -g -o $(GOAL) $(RPSOBJS:.o=.c) $(INC) $(LIBS)

$(GOAL).purify: $(RPSOBJS)
	purify -cache-dir=/tmp gcc -Wall $(RPSOBJS) -g -o $(GOAL).purify $(LIBINC) $(LIBS) $(INC)

# c/n1/i386/Makefile 6/9/94
# Execute tabgen to build new code tables in current directory.
# Expects $(TFLAGS) to be set accordingly.

ALL=\
	prefac.f \
	aadd.t \
	add.t \
	adiv.t \
	aft.t \
	alogic.t \
	amul.t \
	and.t \
	arem.t \
	ashift.t \
	assign.t \
	asub.t \
	bef.t \
	div.t \
	leaves.t \
	mul.t \
	neg.t \
	not.t \
	orxor.t \
	relop.t \
	rem.t \
	shift.t \
	sub.t

macros.c patern.c: $(ALL)
	tabgen $(TFLAGS) $(ALL)

# end of c/n1/i386/Makefile

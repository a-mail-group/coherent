# c/common/Makefile 6/9/94

COM=\
	common.a(bget.$X) \
	common.a(bput.$X) \
	common.a(dget.$X) \
	common.a(diag.$X) \
	common.a(dput.$X) \
	common.a(getvar.$X) \
	common.a(iget.$X) \
	common.a(iput.$X) \
	common.a(lget.$X) \
	common.a(lput.$X) \
	common.a(makvar.$X) \
	common.a(milnam.$X) \
	common.a(mionam.$X) \
	common.a(newlab.$X) \
	common.a(newseg.$X) \
	common.a(nput.$X) \
	common.a(segnam.$X) \
	common.a(sget.$X) \
	common.a(sput.$X) \
	common.a(talloc.$X) \
	common.a(tcpy.$X) \
	common.a(unbget.$X)
COMMCH=\
	common.a(geno.$X) \
	common.a(mdlnam.$X) \
	common.a(mdonam.$X) \
	common.a(regnam.$X) \
	common.a(tyname.$X)

# Primary targets.
common.a:		$(COM) $(COMMCH)
	: common.a done

lib.olb:	$(COM) $(COMMCH)
	lb c $@
	lb r $@ $(COM) $(COMMCH)

lib.lib:	$(COM) $(COMMCH)
	mwlib c lib $(COM) $(COMMCH)

# end of c/common/Makefile

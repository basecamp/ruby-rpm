/* -*- mode: C; c-basic-offset: 4; tab-width: 4; -*- */
/* Ruby/RPM
 *
 * Copyright (C) 2002 Kenta MURATA <muraken2@nifty.com>.
 */

/* $Id: spec.c 44 2004-06-03 16:58:10Z kazuhiko $ */

#include "private.h"

VALUE rpm_cSpec;

static ID id_ba;
static ID id_br;
static ID id_bc;
static ID id_src;
static ID id_pkg;
static ID id_rest;

#if RPM_VERSION(5,0,0) <= RPM_VERSION_CODE
#define headerGetEntryMinMemory(hdr,tag,type,ptr,cnt) \
        headerGetEntry(hdr,tag,type,(void**)ptr,cnt)
#endif

#if RPM_VERSION_CODE < RPM_VERSION(4,1,0)
static void
spec_free(Spec rspec)
{
	freeSpec(rspec);
}

#else
static void
ts_free(rpmts ts)
{
	ts = rpmtsFree(ts);
}

#endif

/*
 * Create a spec file object from a spec file
 * @param [String] filename Spec file path
 * @return [Spec]
 */
static VALUE
spec_s_open(VALUE klass, VALUE filename)
{
#if RPM_VERSION_CODE < RPM_VERSION(4,1,0)
	Spec rspec;
#elif RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
	rpmts ts = NULL;
#else
	rpmSpec spec = NULL;
	rpmSpecFlags flags = (RPMSPEC_ANYARCH|RPMSPEC_FORCE);
#endif

	if (TYPE(filename) != T_STRING) {
		rb_raise(rb_eTypeError, "illegal argument type");
	}

#if RPM_VERSION_CODE < RPM_VERSION(4,1,0)
	switch (parseSpec(&rspec, RSTRING_PTR(filename), "/", NULL, 0, "", NULL, 1, 1)) {
	case 0:
		if (rspec != NULL) {
			break;
		}

	default:
		rb_raise(rb_eRuntimeError, "specfile `%s' parsing failed", RSTRING_PTR(filename));
	}
	return Data_Wrap_Struct(klass, NULL, spec_free, rspec);
#elif RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
	ts = rpmtsCreate();
#if RPM_VERSION_CODE < RPM_VERSION(4,4,8)
	switch (parseSpec(ts, RSTRING_PTR(filename), "/", NULL, 0, "", NULL, 1, 1)) {
#elif RPM_VERSION_CODE < RPM_VERSION(4,5,90)
	switch (parseSpec(ts, RSTRING_PTR(filename), "/", 0, "", NULL, 1, 1, 0)) {
#elif RPM_VERSION_CODE < RPM_VERSION(5,0,0)
	switch (parseSpec(ts, RSTRING_PTR(filename), "/", NULL, 0, "", NULL, 1, 1)) {
#else
	switch (parseSpec(ts, RSTRING_PTR(filename), "/", 0, "", NULL, 1, 1, 0)) {
#endif
	case 0:
		if (ts != NULL) {
			break;
		}

	default:
		rb_raise(rb_eRuntimeError, "specfile `%s' parsing failed", RSTRING_PTR(filename));
	}
	return Data_Wrap_Struct(klass, NULL, ts_free, ts);
#else
	spec = rpmSpecParse(RSTRING_PTR(filename), flags, NULL);
	if (spec == NULL) {
		rb_raise(rb_eRuntimeError, "specfile `%s' parsing failed", RSTRING_PTR(filename));
	}
	return Data_Wrap_Struct(klass, NULL, rpmSpecFree, spec);
#endif
}

/*
 *
 * Alias for Spec#open
 */
VALUE
rpm_spec_open(const char* filename)
{
	return spec_s_open(rpm_cSpec, rb_str_new2(filename));
}

/*
 * @return [String] Return Build root defined in the spec file
 */
VALUE
rpm_spec_get_buildroot(VALUE spec)
{
#if RPM_VERSION_CODE < RPM_VERSION(4,5,90)
	if (RPM_SPEC(spec)->buildRootURL) {
		return rb_str_new2(RPM_SPEC(spec)->buildRootURL);
	}
#elif RPM_VERSION_CODE < RPM_VERSION(4,5,90)
	if (RPM_SPEC(spec)->rootURL) {
		return rb_str_new2(RPM_SPEC(spec)->rootURL);
	}
#elif RPM_VERSION_CODE < RPM_VERSION(4,9,0)
	if (RPM_SPEC(spec)->buildRoot) {
		return rb_str_new2(RPM_SPEC(spec)->buildRoot);
	}
#elif RPM_VERSION_CODE < RPM_VERSION(5,0,0)
	char *buildRootURL = rpmGenPath(NULL, "%{buildroot}", NULL);
	VALUE result = rb_str_new2(buildRootURL);
	free(buildRootURL);
	return result;
#else
	const char *buildRootURL = rpmGenPath(RPM_SPEC(spec)->rootURL, "%{?buildroot}", NULL);
	VALUE result = rb_str_new2(buildRootURL);
	buildRootURL = _free(buildRootURL);
	return result;
#endif
	return Qnil;
}

/*
 * @return [String] Return Build subdirectory defined in the spec file
 */
VALUE
rpm_spec_get_buildsubdir(VALUE spec)
{
#if RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
	if (RPM_SPEC(spec)->buildSubdir) {
		return rb_str_new2(RPM_SPEC(spec)->buildSubdir);
	}
#else
	char *buildSubDir = rpmGenPath(NULL, "%{buildsubdir}", NULL);
	VALUE result = rb_str_new2(buildSubDir);
	free(buildSubDir);
	return result;
#endif
	return Qnil;
}

/*
 * @return [Array<String>] Return Build architectures defined in the spec file
 */
VALUE
rpm_spec_get_buildarchs(VALUE spec)
{
	VALUE ba = rb_ivar_get(spec, id_ba);

	if (NIL_P(ba)) {
		ba = rb_ary_new();
#if RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
		register int i;
		for (i = 0; i < RPM_SPEC(spec)->BACount; i++) {
			rb_ary_push(ba, rb_str_new2(RPM_SPEC(spec)->BANames[i]));
		}
#else
		Header header = rpmSpecSourceHeader(RPM_SPEC(spec));
		rpmtd archtd = rpmtdNew();
		if (!headerGet(header,
                       RPMTAG_BUILDARCHS, archtd, HEADERGET_MINMEM)) {
			rpmtdFree(archtd);
			return ba;
		}
		rpmtdInit(archtd);
	        while ( rpmtdNext(archtd) != -1 ) {
			rb_ary_push(ba, rb_str_new2(rpmtdGetString(archtd)));
		}
#endif
		rb_ivar_set(spec, id_ba, ba);
	}
	return ba;
}

/*
 * @return [Array<RPM::Require>] Return Build requires defined in the spec file
 */
VALUE
rpm_spec_get_buildrequires(VALUE spec)
{
	VALUE br = rb_ivar_get(spec, id_br);

#if RPM_VERSION_CODE < RPM_VERSION(4,6,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
	if (NIL_P(br)) {
		const char** names;
		const char** vers;
		int_32* flags;
		int_32 count;
		rpmTagType nt, vt, type;
		register int i;

		br = rb_ary_new();
		if (!headerGetEntryMinMemory(RPM_SPEC(spec)->buildRestrictions,
									 RPMTAG_REQUIRENAME, (hTYP_t)&nt,
									 (hPTR_t*)&names, (hCNT_t)&count)) {
			goto leave;
		}

		get_entry(RPM_SPEC(spec)->buildRestrictions, RPMTAG_REQUIREVERSION,
				  &vt, (void*)&vers);
		get_entry(RPM_SPEC(spec)->buildRestrictions, RPMTAG_REQUIREFLAGS,
				  &type, (void*)&flags);

		for (i = 0; i < count; i++) {
			rb_ary_push(br, rpm_require_new(names[i], rpm_version_new(vers[i]),
											flags[i], spec));
		}

		release_entry(nt, names);
		release_entry(vt, vers);

		rb_ivar_set(spec, id_br, br);
	}

 leave:
	return br;
#else
	rpmtd nametd = rpmtdNew();
	rpmtd versiontd = rpmtdNew();
	rpmtd flagtd = rpmtdNew();

	if (NIL_P(br)) {
#if RPM_VERSION_CODE >= RPM_VERSION(4,9,0)
		Header header = rpmSpecSourceHeader(RPM_SPEC(spec));
#else
		Header header = RPM_SPEC(spec)->buildRestrictions;
#endif
		br = rb_ary_new();
		if (!headerGet(header,
                       RPMTAG_REQUIRENAME, nametd, HEADERGET_MINMEM)) {
			goto leave;
		}

		get_entry(header, RPMTAG_REQUIREVERSION,
				  versiontd);
		get_entry(header, RPMTAG_REQUIREFLAGS,
				  flagtd);

		rpmtdInit(nametd);
        while ( rpmtdNext(nametd) != -1 ) {
			rb_ary_push(br, rpm_require_new(rpmtdGetString(nametd), rpm_version_new(rpmtdNextString(versiontd)), *rpmtdNextUint32(flagtd), spec));
		}
		rb_ivar_set(spec, id_br, br);
	}

 leave:
	rpmtdFree(nametd);
	rpmtdFree(versiontd);
	rpmtdFree(flagtd);

	return br;
#endif
}

/*
 * @return [Array<RPM::Conflict>] Return Build conflicts defined in the spec file
 */
VALUE
rpm_spec_get_buildconflicts(VALUE spec)
{
	VALUE bc = rb_ivar_get(spec, id_bc);
#if RPM_VERSION_CODE < RPM_VERSION(4,6,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
	if (NIL_P(bc)) {
		const char** names;
		const char** vers;
		int_32* flags;
		int_32 count;
		rpmTagType nt, vt, type;
		register int i;

		bc = rb_ary_new();
		if (!headerGetEntryMinMemory(RPM_SPEC(spec)->buildRestrictions,
									 RPMTAG_CONFLICTNAME, (hTYP_t)&nt,
									 (hPTR_t*)&names, (hCNT_t)&count)) {
			goto leave;
		}

		get_entry(RPM_SPEC(spec)->buildRestrictions, RPMTAG_CONFLICTVERSION,
				  &vt, (void*)&vers);
		get_entry(RPM_SPEC(spec)->buildRestrictions, RPMTAG_CONFLICTFLAGS,
				  &type, (void*)&flags);

		for (i = 0; i < count; i++) {
			rb_ary_push(bc, rpm_conflict_new(names[i], rpm_version_new(vers[i]),
											 flags[i], spec));
		}

		release_entry(nt, names);
		release_entry(vt, vers);

		rb_ivar_set(spec, id_bc, bc);
	}
 leave:
	return bc;
#else
	rpmtd nametd = rpmtdNew();
	rpmtd versiontd = rpmtdNew();
	rpmtd flagtd = rpmtdNew();

	if (NIL_P(bc)) {
#if RPM_VERSION_CODE >= RPM_VERSION(4,9,0)
		Header header = rpmSpecSourceHeader(RPM_SPEC(spec));
#else
		Header header = RPM_SPEC(spec)->buildRestrictions;
#endif
		bc = rb_ary_new();
		if (!headerGet(header,
                       RPMTAG_CONFLICTNAME, nametd, HEADERGET_MINMEM)) {

			goto leave;
		}

		get_entry(header, RPMTAG_CONFLICTVERSION,
				  versiontd);
		get_entry(header, RPMTAG_CONFLICTFLAGS,
				  flagtd);

		rpmtdInit(nametd);
		while ( rpmtdNext(nametd) != -1) {
			rb_ary_push(bc, rpm_conflict_new(rpmtdGetString(nametd), rpm_version_new(rpmtdNextString(versiontd)), *rpmtdNextUint32(flagtd), spec));
		}

		rb_ivar_set(spec, id_bc, bc);
	}
 leave:
	rpmtdFree(nametd);
	rpmtdFree(versiontd);
	rpmtdFree(flagtd);

	return bc;
#endif
}

VALUE
rpm_spec_get_build_restrictions(VALUE spec)
{
	VALUE cache = rb_ivar_get(spec, id_rest);

	if (NIL_P(cache)) {
#if RPM_VERSION_CODE >= RPM_VERSION(4,9,0) && RPM_VERSION_CODE < RPM_VERSION(5,0,0)
		Header header = rpmSpecSourceHeader(RPM_SPEC(spec));
#else
		Header header = RPM_SPEC(spec)->buildRestrictions;
#endif
		cache = rpm_package_new_from_header(header);
		rb_ivar_set(spec, id_rest, cache);
	}

	return cache;
}

/*
 * @return [Array<RPM::Source>] Sources defined in the spec file
 */
VALUE
rpm_spec_get_sources(VALUE spec)
{
	VALUE src = rb_ivar_get(spec, id_src);

	if (NIL_P(src)) {
		src = rb_ary_new();
#if RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
		struct Source* s = RPM_SPEC(spec)->sources;

		while (s != NULL) {
			VALUE obj = Qnil;

			if (s->flags & RPMBUILD_ISSOURCE) {
				obj = rpm_source_new(s->fullSource, s->num, s->flags & RPMBUILD_ISNO);
			} else if (s->flags & RPMBUILD_ISPATCH) {
				obj = rpm_patch_new(s->fullSource, s->num, s->flags & RPMBUILD_ISNO);
			} else if (s->flags & RPMBUILD_ISICON) {
				obj = rpm_icon_new(s->fullSource, s->num, s->flags & RPMBUILD_ISNO);
			}

			rb_ary_push(src, obj);
			s = s->next;
		}
#else
		rpmSpecSrcIter s = rpmSpecSrcIterInit(RPM_SPEC(spec));
		rpmSpecSrc source;
		while ((source = rpmSpecSrcIterNext(s)) != NULL) {
			VALUE obj = Qnil;
			rpmSourceFlags flags = rpmSpecSrcFlags(source);
			if (flags & RPMBUILD_ISSOURCE) {
				obj = rpm_source_new(rpmSpecSrcFilename(source, 1),
						     rpmSpecSrcNum(source), flags & RPMBUILD_ISNO);
			} else if (flags & RPMBUILD_ISPATCH) {
				obj = rpm_patch_new(rpmSpecSrcFilename(source, 1),
						    rpmSpecSrcNum(source), flags & RPMBUILD_ISNO);
			} else if (flags & RPMBUILD_ISICON) {
				obj = rpm_icon_new(rpmSpecSrcFilename(source, 1),
						   rpmSpecSrcNum(source), flags & RPMBUILD_ISNO);
			}

			rb_ary_push(src, obj);
		}
		rpmSpecSrcIterFree(s);
#endif
		rb_ivar_set(spec, id_src, src);
	}

	return src;
}

/*
 * @return [Array<RPM::Package>] Packages defined in the spec file
 */
VALUE
rpm_spec_get_packages(VALUE spec)
{
	VALUE pkg = rb_ivar_get(spec, id_pkg);

	if (NIL_P(pkg)) {
		pkg = rb_ary_new();
#if RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
		Package p = RPM_SPEC(spec)->packages;

		while (p != NULL) {
			if (p->fileList)
				rb_ary_push(pkg, rpm_package_new_from_header(p->header));
			p = p->next;
		}
#else
		rpmSpecPkgIter pi = rpmSpecPkgIterInit(RPM_SPEC(spec));
		rpmSpecPkg p;
		while ((p = rpmSpecPkgIterNext(pi)) != NULL) {
			const char * fileList = rpmSpecGetSection(RPM_SPEC(spec), RPMBUILD_FILECHECK);
			Header header = rpmSpecPkgHeader(p);
			rb_ary_push(pkg, rpm_package_new_from_header(header));
		}
		rpmSpecPkgIterFree(pi);
#endif
		rb_ivar_set(spec, id_pkg, pkg);
	}

	return pkg;
}

/*
 * Builds the spec file
 * @param [Number] flags bits to enable stages of build
 * @param [Boolean] test When true, don't execute scripts or package
 * @return [Number] exit code
 *
 * @example
 *   spec = RPM::Spec.open("foo.spec")
 *   spec.build(RPM::BUILD__UNTIL_BUILD)
 */
VALUE
rpm_spec_build(int argc, VALUE* argv, VALUE spec)
{
	int flags, test;
	rpmRC rc;

	switch (argc) {
	case 0:
		rb_raise(rb_eArgError, "argument too few(1..2)");

	case 1:
		flags = NUM2INT(argv[0]);
		test = 0;
		break;

	case 2:
		flags = NUM2INT(argv[0]);
		test = RTEST(argv[1]);
		break;

	default:
		rb_raise(rb_eArgError, "argument too many(0..1)");
	}

#if RPM_VERSION_CODE < RPM_VERSION(4,1,0)
	rc = buildSpec(RPM_SPEC(spec), flags,test);
#elif RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
	rpmts ts = NULL;
	ts = rpmtsCreate();
	rc = buildSpec(ts, RPM_SPEC(spec), flags,test);
	ts_free(ts);
#else
	struct rpmBuildArguments_s buildArgs;
	buildArgs.buildAmount = flags;
	if (test)
	       	buildArgs.buildAmount |= RPMBUILD_NOBUILD;
	rc = rpmSpecBuild(RPM_SPEC(spec), &buildArgs);
#endif

	return INT2NUM(rc);
}

/*
 * Expands a macro in the spec file
 * @param [String] name Name of the macro
 * @return [String]
 * @example
 *    spec.expand_macros("configure")
 *    => "\n CFLAGS=......."
 *    spec.expand_macros("_prefix")
 *    => "/usr"
 */
VALUE
rpm_spec_expand_macros(VALUE spec, VALUE name)
{
	char  buf[BUFSIZ];
	char *tmp, *res;
	VALUE val;

	if (TYPE(name) != T_STRING) {
		rb_raise(rb_eTypeError, "illegal argument type");
	}
	sprintf(buf, "%%{%s}", RSTRING_PTR(name));
#if RPM_VERSION_CODE < RPM_VERSION(4,9,0) || RPM_VERSION_CODE >= RPM_VERSION(5,0,0)
	tmp = strdup(buf);
	expandMacros(RPM_SPEC(spec), RPM_SPEC(spec)->macros, buf, BUFSIZ);
	res = buf;
#else
	tmp = rpmExpand(buf, NULL);
	res = tmp;
#endif
	if (strcmp(tmp, buf) == 0) {
		val = Qnil;
	} else {
		val = rb_str_new2(res);
	}
	free(tmp);
	return val;
}

void
Init_rpm_spec(void)
{
	rpm_cSpec = rb_define_class_under(rpm_mRPM, "Spec", rb_cData);
	rb_define_singleton_method(rpm_cSpec, "open", spec_s_open, 1);
	rb_define_singleton_method(rpm_cSpec, "new", spec_s_open, 1);
	rb_define_method(rpm_cSpec, "buildroot", rpm_spec_get_buildroot, 0);
	rb_define_method(rpm_cSpec, "buildsubdir", rpm_spec_get_buildsubdir, 0);
	rb_define_method(rpm_cSpec, "buildarchs", rpm_spec_get_buildarchs, 0);
	rb_define_method(rpm_cSpec, "buildrequires", rpm_spec_get_buildrequires, 0);
	rb_define_method(rpm_cSpec, "buildconflicts", rpm_spec_get_buildconflicts, 0);
	rb_define_method(rpm_cSpec, "build_restrictions", rpm_spec_get_build_restrictions, 0);
	rb_define_method(rpm_cSpec, "sources", rpm_spec_get_sources, 0);
	rb_define_method(rpm_cSpec, "packages", rpm_spec_get_packages, 0);
	rb_define_method(rpm_cSpec, "build", rpm_spec_build, -1);
	rb_define_method(rpm_cSpec, "expand_macros", rpm_spec_expand_macros, 1);
	rb_undef_method(rpm_cSpec, "dup");
	rb_undef_method(rpm_cSpec, "clone");

	id_ba = rb_intern("buildarchs");
	id_br = rb_intern("buildrequires");
	id_bc = rb_intern("buildconflicts");
	id_src = rb_intern("sources");
	id_pkg = rb_intern("packages");
	id_rest = rb_intern("build_restrictions");
}

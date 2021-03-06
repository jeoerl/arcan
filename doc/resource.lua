-- resource
-- @short: Check if the requested resource path exists.
-- @inargs: name, *domain*
-- @outargs: longname
-- @longdescr: This function tries to resolve the path description
-- indicated by name and optionally filtered by domain. The accepted
-- domain values follow the same rules as other resource related functions
-- e.g. glob_resource (APPL_RESOURCE, APPL_TEMP_RESOURCE,
-- SHARED_RESOURCE, SYS_APPL_RESOURCE). By default, all user- accessible
-- domains are scanned and the resolved result (or nil) is returned
-- as *longname*.
-- @group: resource
-- @cfunction: resource
function main()
#ifdef MAIN
	print( resource("images/icons/arcanicon.png.png") );
	print( resource("test.png", APPL_RESOURCE) );
#endif
end

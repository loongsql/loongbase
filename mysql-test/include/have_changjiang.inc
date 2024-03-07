##
## Checks for the presence of the Changjiang storage engine (plugin)
## by querying the INFORMATION_SCHEMA.ENGINES table.
##
## If Changjiang is not available, the test using this include file
## is skipped.
##

# Do not echo these statements to test results
--disable_query_log

# Check if Changjiang is available
let $have_changjiang = `SELECT (support = 'YES' OR support = 'DEFAULT' OR support = 'ENABLED') AS 'HAVE_CHANGJIANG' FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'CHANGJIANG'`;
if (!$have_changjiang)
{
  skip Test needs the Changjiang storage engine;
}
--enable_query_log

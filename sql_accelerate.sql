CREATE OR REPLACE FUNCTION
  get_data( TEXT )
RETURNS
  bigint
AS
  'sql_accelerate.so', 'get_data'
LANGUAGE
  C
STRICT
IMMUTABLE;

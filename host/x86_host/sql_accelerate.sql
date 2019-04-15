CREATE OR REPLACE FUNCTION
  get_data( TEXT )
RETURNS
  real
AS
  'sql_accelerate.so', 'get_data'
LANGUAGE
  C
STRICT
IMMUTABLE;

<?php

/**
 * ADOdb Lite is a PHP class to encapsulate multiple database APIs and is compatible with 
 * a subset of the ADODB Command Syntax. 
 * Currently supports Frontbase, MaxDB, miniSQL, MSSQL, MSSQL Pro, MySQLi, MySQLt, MySQL, PostgresSQL,
 * PostgresSQL64, PostgresSQL7, SqLite and Sybase.
 * 
 */

class mssqlnative_driver_ADOConnection extends ADOConnection
{
	var $sysDate = 'convert(datetime,convert(char,GetDate(),102),102)';
	var $sysTimeStamp = 'GetDate()';
	var $hasTop = 'top';		// support mssql SELECT TOP 10 * FROM TABLE

	function mssqlnative_driver_ADOConnection()
	{
		$this->dbtype = 'mssqlnative';
		$this->dataProvider = 'mssqlnative';
	}

	/**
	 * Connection to database server and selected database
	 * 
	 * @access private 
	 */

	function _connect($host = "", $username = "", $password = "", $database = "", $persistent, $forcenew)
	{
		if (!function_exists('sqlsrv_connect')) return null;
        $connectionInfo = array("Database"=>$database,'UID'=>$username,'PWD'=>$password);
        if ($this->debug) error_log("<hr>connecting... hostname: $argHostname params: ".var_export($connectionInfo,true));
        if(!($this->connectionID = sqlsrv_connect($host,$connectionInfo))) { 
            if ($this->debug) error_log( "<hr><b>errors</b>: ".print_r( sqlsrv_errors(), true));
			if ($fn = $this->raiseErrorFn) 
				$fn($this->dbtype, 'CONNECT', $this->ErrorNo(), $this->ErrorMsg(), $this->host, $this->database, $this);
            return false;
        }
		return true;	
	} 

	/**
	 * Choose a database to connect.
	 *
	 * @param dbname 	is the name of the database to select
	 * @return 		true or false
	 * @access public
	 */

	function SelectDB($dbname)
	{
		$this->database = $dbName;
		$this->databaseName = $dbName; # obsolete, retained for compat with older adodb versions
		if ($this->connectionID) {
            $rs = $this->Execute('USE '.$dbName); 
            if($rs) {
                return true;
            } else return false;		
		}
		else return false;	
	} 

	/**
	 * Return database error message
	 * Usage: $errormessage =& $db->ErrorMsg();
	 * 
	 * @access public
	 */

	function ErrorMsg()
	{
            $retErrors = sqlsrv_errors(SQLSRV_ERR_ALL);
            if($retErrors != null) {
			    foreach($retErrors as $arrError) {
				    $this->_errorMsg .= "SQLState: ".$arrError[ 'SQLSTATE']."\n";
				    $this->_errorMsg .= "Error Code: ".$arrError[ 'code']."\n";
				    $this->_errorMsg .= "Message: ".$arrError[ 'message']."\n";
			    }
            } else {
                $this->_errorMsg = "No errors found";
            }
		return $this->_errorMsg;
	}

	/**
	 * Return database error number
	 * Usage: $errorbo =& $db->ErrorNo();
	 * 
	 * @access public
	 */

	function ErrorNo()
	{
		if ($this->_logsql && $this->_errorCode !== false) return $this->_errorCode;
		$err = sqlsrv_errors(SQLSRV_ERR_ALL);
        if($err[0]) return $err[0]['code'];
        else return -1;
	}

	/**
	 * Returns # of affected rows from insert/delete/update query
	 * 
	 * @access public 
	 * @return integer Affected rows
	 */

	function Affected_Rows()
	{
        return sqlsrv_rows_affected($this->_queryID);
	} 

	/**
	 * Returns the last record id of an inserted item
	 * Usage: $db->Insert_ID();
	 * 
	 * @access public 
	 */

	function Insert_ID()
	{
		return $this->GetOne($this->identitySQL);
	}

	/**
	 * Correctly quotes a string so that all strings are escape coded.
	 * An example is  $db->qstr("Haven't a clue.");
	 * 
	 * @param string			the string to quote
	 * @param [magic_quotes]	if $s is GET/POST var, set to get_magic_quotes_gpc().
	 *
	 * @return  single-quoted string IE: 'Haven\'t a clue.'
	 */

	function qstr($string, $magic_quotes=false)
	{	
		if (!$magic_quotes) {
			return  "'".str_replace("'", "''", $string)."'";
		}
		$string = str_replace("\\'", "''", str_replace('\\\\', '\\', str_replace('\\"', '"', $string)));
		return "'" . $string . "'";
	}

	function QMagic($string)
	{
		return $this->qstr($string, get_magic_quotes_gpc());
	}

	/**
	 * Returns concatenated string
	 * Usage: $db->Concat($str1,$str2);
	 * 
	 * @return concatenated string
	 */
	function Concat()
	{
		$s = "";
		$arr = func_get_args();

		if (sizeof($arr) == 1) {
			foreach ($arr as $arg) {
				$args = explode(',', $arg);
			}
			$arr = $args;
		}

		array_walk($arr, create_function('&$v', '$v = "CAST(" . $v . " AS VARCHAR(255))";'));
		$s = implode('+',$arr);
		if (sizeof($arr) > 0) return "$s";
		return '';
	}

	function IfNull( $field, $ifNull ) 
	{
		return " ISNULL($field, $ifNull) ";
	}

	/**
	 * Closes database connection
	 * Usage: $db->close();
	 * 
	 * @access public 
	 */

	function Close()
	{
		if ($this->transCnt) $this->RollbackTrans();
		$rez = @sqlsrv_close($this->connectionId);
		$this->connectionId = false;
		return $rez;
	}

	 /**
	 * Returns All Records in an array
	 *
	 * Usage: $db->GetAll($sql);
	 * @access public 
	 */

	function &GetAll($sql, $inputarr = false)
	{
		$data =& $this->GetArray($sql, $inputarr);
		return $data;
	}

	 /**
	 * Returns All Records in an array
	 *
	 * Usage: $db->GetArray($sql);
	 * @access public 
	 */

	function &GetArray($sql, $inputarr = false)
	{
		$data = false;
		$result =& $this->Execute($sql, $inputarr);
		if ($result)
		{
			$data =& $result->GetArray();
			$result->Close();
		}
		return $data;
	}

	/**
	 * Construct a LIMIT query with optional offset
	 * This is used for query pages
	 * $sql string SQL query we will append the limit too
	 * $limit integer the SQL limit
	 * $offset integer the SQL offset (default false)
	 */
	function limitResult($sql, $limit,$offset=false) {
		if($offset === false || $offset == 0) {
			if ( stripos($sql, "SELECT") === false ) {
				return "TOP {$limit} " . $sql;
			} else {
				return preg_replace("/(^\s*select\s+(distinctrow|distinct)?)/i", "\\1 TOP {$limit}", $sql);
			}		
		} else {
		    $orderby = stristr($sql, 'ORDER BY');
	        if ($orderby !== false) $order = str_ireplace('ORDER BY', '', $orderby);
	        $sql = preg_replace('/(^\s*select\s+(distinctrow|distinct)?)/i', '\\1 TOP(10000000) ', $sql);
	        $sql = '
	        	SELECT * FROM (
	        	  SELECT sub2.*, ROW_NUMBER() OVER(ORDER BY sub2.line2) AS line3 FROM (
	        	    SELECT 1 AS line2, sub1.* FROM (' . $sql . ') AS sub1
	        	  ) as sub2
	        	) AS sub3 
	        	WHERE line3 BETWEEN ' . ($limit) . ' AND ' . ($offset + $limit);
	        if ($orderby !== false) {
	        	$sql .= ' ORDER BY ' . $order;
	        }			
	        return $sql;
		}
	}
	
	/**
	 * Executes SQL query and instantiates resultset methods
	 * 
	 * @access private 
	 * @return mixed Resultset methods
	 */

	function &do_query( $sql, $offset, $nrows, $inputarr=false )
	{
		global $ADODB_FETCH_MODE;

		$false = false;

		if ($nrows > 0 || $offset > 0) {
			$sql = $this->limitResult($sql, $nrows,$offset);
		}
		if(strpos($sql,"\\'") !== false) $sql = str_replace("\\'","''",$sql);//fix for inserts/queries using mysql formated quoted strings
		
		if ($inputarr && is_array($inputarr)) {
			$sqlarr = explode('?', $sql);
			if (!is_array(reset($inputarr))) $inputarr = array($inputarr);
			foreach($inputarr as $arr) {
				$sql = ''; $i = 0;
				foreach($arr as $v) {
					$sql .= $sqlarr[$i];
					switch(gettype($v)){
						case 'string':
							$sql .= $this->qstr($v);
							break;
						case 'double':
							$sql .= str_replace(',', '.', $v);
							break;
						case 'boolean':
							$sql .= $v ? 1 : 0;
							break;
						default:
							if ($v === null)
								$sql .= 'NULL';
							else $sql .= $v;
					}
					$i += 1;
				}
				$sql .= $sqlarr[$i];
				if ($i+1 != sizeof($sqlarr))	
					return $false;
				$this->sql = $sql;
				$time_start = array_sum(explode(' ', microtime()));
				$this->query_count++;
				$resultId = @sqlsrv_query( $this->connectionId, $this->sql );
				$time_total = (array_sum(explode(' ', microtime())) - $time_start);
				$this->query_time_total += $time_total;
				if($this->debug_console)
				{
					$this->query_list[] = $this->sql;
					$this->query_list_time[] = $time_total;
					$this->query_list_errors[] = $this->ErrorMsg();
				}
				if($this->debug)
				{
					$this->outp($sql);
				}
				if ($resultId === false) { // error handling if query fails
					if ($fn = $this->raiseErrorFn)
						$fn($this->dbtype, 'EXECUTE', $this->ErrorNo(), $this->ErrorMsg(), $this->sql, $inputarr, $this);
					return $false;
				} 

			}
		}
		else
		{
				$this->sql = $sql;
				$time_start = array_sum(explode(' ', microtime()));
				$this->query_count++;
				$resultId = @sqlsrv_query( $this->connectionId, $this->sql );
				$time_total = (array_sum(explode(' ', microtime())) - $time_start);
				$this->query_time_total += $time_total;
				if($this->debug_console)
				{
					$this->query_list[] = $this->sql;
					$this->query_list_time[] = $time_total;
					$this->query_list_errors[] = $this->ErrorMsg();
				}
				if($this->debug)
				{
					$this->outp($sql);
				}
		}

		if ($resultId === false) { // error handling if query fails
			if ($fn = $this->raiseErrorFn)
				$fn($this->dbtype, 'EXECUTE', $this->ErrorNo(), $this->ErrorMsg(), $this->sql, $inputarr, $this);
			return $false;
		} 

		if (stripos($this->sql,'insert') !== false || stripos($this->sql,'update') !== false || stripos($this->sql,'delete') !== false) { // return simplified recordset for inserts/updates/deletes with lower overhead
			$recordset = new ADORecordSet_empty();
			return $recordset;
		}

		$resultset_name = $this->last_module_name . "_ResultSet";
		$recordset = new $resultset_name( $resultId, $this->connectionId );

		return $recordset;
	} 
} 

class mssqlnative_driver_ResultSet
{
	var $connectionId;
	var $fields;
	var $fieldmeta;
	var $fieldmeta_offset;
	var $rows;
	var $resultId;
	var $_currentRow = 0;
	var $_numOfRows = -1;
	var $_numOfFields = -1;
	var $fetchMode;
	var $EOF;

	/**
	 * mssqlResultSet Constructor
	 * 
	 * @access private 
	 * @param string $record 
	 * @param string $resultId 
	 */

	function mssql_driver_ResultSet( $resultId, $connectionId )
	{
		$this->fields = array();
		$this->rows = array();
		$this->connectionId = $connectionId;
		$this->resultId = $resultId;
		$this->EOF = false;

		$this->_currentRow = 0;

		$this->_numOfFields = @sqlsrv_num_fields( $resultId );
		$this->fieldmeta = @sqlsrv_field_metadata( $resultId );
		$this->fieldmeta_offset = 0;
		
		switch ($ADODB_FETCH_MODE) {
			case ADODB_FETCH_NUM: $this->fetchMode = SQLSRV_FETCH_NUMERIC; break;
			case ADODB_FETCH_ASSOC:$this->fetchMode = SQLSRV_FETCH_ASSOC; break;
			default:
			case ADODB_FETCH_DEFAULT:
			case ADODB_FETCH_BOTH:$this->fetchMode = SQLSRV_FETCH_BOTH; break;
		}

		while( $row = sqlsrv_fetch_array( $resultId, $this->fetchMode)) {//numrows is not implemented, read results to an array and emulate
			$this->rows[] = $row;
		}
		sqlsrv_free_stmt( $resultId);//don't need the real one anymore

		$recordset->_numOfRows = count($this->rows);
		if( $recordset->_numOfRows == 0) {
			$recordset->EOF = true;
		}
	} 

	/**
	 * Frees resultset
	 * 
	 * @access public 
	 */

	function Close()
	{
		$this->fields = array();
		$this->rows = array();
		$this->resultId = false;
	} 

	/**
	 * Returns field name from select query
	 * 
	 * @access public 
	 * @param string $field
	 * @return string Field name
	 */

	function fields( $field )
	{
		if(empty($field))
		{
			return $this->fields;
		}
		else
		{
			return $this->fields[$field];
		}
	} 

	/**
	 * Returns numrows from select query
	 * 
	 * @access public 
	 * @return integer Numrows
	 */

	function RecordCount()
	{
		return $this->_numOfRows;
	} 

	/**
	 * Returns num of fields from select query
	 * 
	 * @access public 
	 * @return integer numfields
	 */

	function FieldCount()
	{
		return $this->_numOfFields;
	} 

	/**
	 * Returns next record
	 * 
	 * @access public 
	 */

	function MoveNext()
	{
		$this->_currentRow++;
		if (array_key_exists($this->rows,$this->_currentRow)) {
			$this->fields = $this->rows[$this->_currentRow];
			return true;
		} else {
			$this->EOF = true;
			return false;
		}
	} 

	/**
	 * Move to the first row in the recordset. Many databases do NOT support this.
	 *
	 * @return true or false
	 */

	function MoveFirst() 
	{
		if ($this->_currentRow == 0) return true;
		return $this->Move(0);			
	}			

	/**
	 * Returns the Last Record
	 * 
	 * @access public 
	 */

	function MoveLast()
	{
		if ($this->EOF) return false;
		return $this->Move($this->_numOfRows - 1);
	} 

	/**
	 * Random access to a specific row in the recordset. Some databases do not support
	 * access to previous rows in the databases (no scrolling backwards).
	 *
	 * @param rowNumber is the row to move to (0-based)
	 *
	 * @return true if there still rows available, or false if there are no more rows (EOF).
	 */

	function Move($rowNumber = 0) 
	{
		if ($rowNumber == $this->_currentRow) return true;
		$this->EOF = false;
   		if ($this->_numOfRows > 0){
			if ($rowNumber >= $this->_numOfRows - 1){
				$rowNumber = $this->_numOfRows - 1;
			}
  		}

		if (array_key_exists($this->rows,$rowNumber)) {
			$this->_currentRow = $rowNumber;
			if ($this->_fetch()) {
				return true;
			}
			$this->fields = false;	
		}
		$this->EOF = true;
		return false;
	}

	/**
	 * Perform Seek to specific row
	 * 
	 * @access private 
	 */

	function _seek($row)
	{
		if ($this->_numOfRows == 0) return false;
		if (array_key_exists($this->rows,$row)) {
			$this->_currentRow = $row;
			$this->fields = array();	
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Fills field array with first database element when query initially executed
	 * 
	 * @access private 
	 */

	function _fetch()
	{
		if (array_key_exists($this->rows,$this->_currentRow)) {
			$this->fields = $this->rows[$this->_currentRow];	
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Check to see if last record reached
	 * 
	 * @access public 
	 */

	function EOF()
	{
		if( $this->_currentRow < $this->_numOfRows) {
			return false;
		} else {
			$this->EOF = true;
			return true;
		}
	} 

	/**
	 * Returns All Records in an array
	 * 
	 * @access public 
	 * @param [nRows]  is the number of rows to return. -1 means every row.
	 */

	function &GetArray($nRows = -1)
	{
		$results = array();
		$cnt = 0;
		while (!$this->EOF && $nRows != $cnt) {
			$results[] = $this->fields;
			$this->MoveNext();
			$cnt++;
		}
		return $results;
	} 

	function &GetRows($nRows = -1) 
	{
		$arr =& $this->GetArray($nRows);
		return $arr;
	}

	function &GetAll($nRows = -1)
	{
		$arr =& $this->GetArray($nRows);
		return $arr;
	}

	/**
	* Fetch field information for a table. 
	*
	* @return object containing the name, type and max_length
	*/
	function FetchField($fieldOffset = -1) 
	{
		if ($fieldOffset != -1) {
			$this->fieldmeta_offset = $fieldOffset;
		}
		$fieldObject = false;
		$i=0;
		foreach($this->fieldmeta as $o) {
			if($i==$this->fieldmeta_offset) {
				$fieldObject = $o;
				break;
			}
			$i++;
		}
		$this->fieldmeta_offset++;
		return $fieldObject;
	}
}
?>
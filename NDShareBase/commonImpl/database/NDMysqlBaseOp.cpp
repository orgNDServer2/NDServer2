#include "database/NDMysqlBaseOp.h"

//#include "NDTypes.h"	//因为mysql.h中要用到SOCKET定义,所以要前置,(这个不需要了,database/NDMysqlBaseOp.h前置了);

#include <mysql.h>

#include "NDShareBaseMacros.h"
#include "NDShareBaseGlobal.h"

#include "thread/NDMutex.h"
#include "file/NDConsoleLog.h"
#include "function/NDRefCounted.h"
#include "database/NDField.h"
#include "database/NDMysqlConst.h"



_NDSHAREBASE_BEGIN

NDMysqlBaseOp::NDMysqlBaseOp() :m_nConnCount(0), m_pHanldeMutex( new NDMutexLock ), m_pConnHandle( NULL ), m_pConnParam( new MysqlConnParam )
{
}

NDMysqlBaseOp::~NDMysqlBaseOp()
{
	disConnectDB();
	SAFE_DELETE( m_pHanldeMutex );
	SAFE_DELETE( m_pConnParam );
}


NDBool NDMysqlBaseOp::initialize( const MysqlConnParam& connParam, NDUint32 nConnCount )
{
	NDBool bRet = NDFalse;

	m_nConnCount	= nConnCount;
	*m_pConnParam	= connParam;
	m_pConnHandle	= new MysqlConn[nConnCount];
	memset( m_pConnHandle, 0, nConnCount * sizeof(MysqlConn) );

	for ( NDUint32 i = 0; i < nConnCount; ++i )
	{
		if ( NDTrue == connectDB( &m_pConnHandle[i] ) )
		{
			m_pConnHandle[i].m_nIndex = i;
			m_pConnHandle[i].m_bBusy  = NDFalse;

			bRet = NDTrue;
		}
		else
		{
			bRet = NDFalse;
			break;
		}
	}

	return bRet;
}

NDBool NDMysqlBaseOp::connectDB( MysqlConn* pConn )
{
	MYSQL* pMysql = mysql_init(NULL);
	if ( NULL == pMysql )
	{
		NDLOG_ERROR << " [NDMysqlBaseOp::connectDB] mysql: mysql_init failed! " << NDLOG_END;
		return NDFalse;
	}

	if ( mysql_options(pMysql, MYSQL_SET_CHARSET_NAME, "utf8" ) )
	{
		NDLOG_ERROR << " [NDMysqlBaseOp::connectDB] mysql: Could not set utf8 character set [!!] " <<NDLOG_END;
		return NDFalse;
	}

	// Set reconnect;
	my_bool my_true = true;
	if ( mysql_options(pMysql, MYSQL_OPT_RECONNECT,  &my_true) )
	{
		NDLOG_ERROR << " [NDMysqlBaseOp::connectDB] mysql: MYSQL_OPT_RECONNECT could not be set, connection drops may occur but will be counteracted." << NDLOG_END;
		return NDFalse;
	}

	//USE COMPRESS PROTOCOL;
	if ( mysql_options(pMysql, MYSQL_OPT_COMPRESS,  0) )
	{
		NDLOG_ERROR << " [NDMysqlBaseOp::connectDB] mysql: MYSQL_OPT_COMPRESS could not be set." << NDLOG_END;
		return NDFalse;
	}

	pConn->m_pMysql = mysql_real_connect( pMysql, m_pConnParam->getHostName().c_str(), m_pConnParam->getUserName().c_str(), m_pConnParam->getPassWord().c_str(),
											"", m_pConnParam->getPort(), NULL, 0 );

	NDBool bRet = NDFalse;
	if ( NULL == pConn->m_pMysql )
	{
		NDLOG_ERROR << " [NDMysqlBaseOp::connectDB] mysql: Connection failed. Reason was " << mysql_error(pMysql) << NDLOG_END;
		return bRet;
	}
	else
	{
		if ( 0 == mysql_select_db( pConn->m_pMysql, m_pConnParam->getDBName().c_str() ) )
		{
			bRet = NDTrue;
		}
		else
		{
			NDLOG_ERROR << " [NDMysqlBaseOp::connectDB] mysql: select of Database " << m_pConnParam->getDBName().c_str() << " failed due to " << mysql_error(pMysql) << NDLOG_END;
		}
	}

	return bRet;
}


void NDMysqlBaseOp::disConnectDB()
{
	NDGuardLock handleLocker( *m_pHanldeMutex );
	for ( NDUint32 i = 0; i < m_nConnCount; ++i )
	{
		disConnectDB( &m_pConnHandle[i] );
	}
	SAFE_DELETE_ARRAY(m_pConnHandle);
}


void NDMysqlBaseOp::disConnectDB( MysqlConn* pConn )
{
	if ( NULL != pConn->m_pMysql )
	{
		mysql_close( pConn->m_pMysql );
		pConn->m_pMysql = NULL;
	}
}


NDBool NDMysqlBaseOp::checkConnections()
{
	return NDTrue;
}

NDMysqlBaseOp::MysqlConn* NDMysqlBaseOp::getIdleHandle()
{
	NDGuardLock handleLocker( *m_pHanldeMutex );
	for ( NDUint32 i = 0; i < m_nConnCount; ++i )
	{
		if ( m_pConnHandle[i].m_bBusy == NDFalse )
		{
			m_pConnHandle[i].m_bBusy = NDTrue;
			return &m_pConnHandle[i];
		}
	}

	return NULL;
}


void NDMysqlBaseOp::setIdleHandle( MysqlConn* pConn )
{
	if ( pConn->m_nIndex < m_nConnCount )
	{
		NDGuardLock handleLocker( *m_pHanldeMutex );
		m_pConnHandle[pConn->m_nIndex].m_bBusy = NDFalse;
	}
}

string NDMysqlBaseOp::escapeString( MysqlConn* pConn, const char* pszBuf, NDUint32 nSize )
{
	char* pBuf = new char[nSize*2 + 1];
	memset( pBuf, 0, nSize*2 + 1 );
	NDRefCounted<char> refBuf( pBuf );

	//NDUint32 nRet = mysql_real_escape_string( pConn->m_pMysql,  pBuf, pszBuf, nSize );
	mysql_real_escape_string( pConn->m_pMysql,  pBuf, pszBuf, nSize );
	
	return string(pBuf);
}

string NDMysqlBaseOp::escapeString( const char* pszBuf, NDUint32 nSize )
{
	if ( (NULL == pszBuf) || ( '\0' == pszBuf[0]) || ( 0 >= nSize) )
	{
		return string("");
	}

	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return string("");
	}

	string strBuf = escapeString( pConn, pszBuf, nSize );
	setIdleHandle( pConn );

	return strBuf;
}

MYSQL_RES* NDMysqlBaseOp::selectSql( const char *szSql , NDUint32 nSize, NDBool bChinese )
{
	if ( (NULL == szSql) || ('\0' == szSql[0]) || (0 >= nSize) )
	{
		return NULL;
	}
	
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return NULL;
	}

	
	if ( NDTrue == bChinese )
	{
		if ( NDFalse == setChineseFont( pConn ) )
		{
			setIdleHandle( pConn );
			return NULL;
		}
	}

	MYSQL_RES* pMysqlRes = NULL;
	if ( mysql_real_query( pConn->m_pMysql, szSql, nSize ) )
	{
		NDLOG_ERROR << " NDMysqlBaseOp::selectSql query failed! " << NDLOG_END;
		NDLOG_ERROR << " mysql query error info:" << mysql_error(pConn->m_pMysql) << NDLOG_END;
	}
	else
	{
		NDUint32 nRow		= (NDUint32)mysql_affected_rows( pConn->m_pMysql );
		NDUint32 nField		= mysql_field_count( pConn->m_pMysql );

		if ( (0 == nRow) || (0 == nField) )
		{
			mysql_free_result( pMysqlRes );
		}
		else
		{
			pMysqlRes = mysql_store_result( pConn->m_pMysql );
		}
	}
	setIdleHandle( pConn );

	return pMysqlRes;
}

NDUint32 NDMysqlBaseOp::updateSql( const char *szSql , NDUint32 nSize, NDBool bChinese )
{
	if ( (NULL == szSql) || ('\0' == szSql[0]) || (0 >= nSize) )
	{
		return 0;
	}

	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return 0;
	}

	if ( NDTrue == bChinese )
	{
		if ( NDFalse == setChineseFont( pConn ) )
		{
			setIdleHandle( pConn );
			return 0;
		}
	}

	NDUint32 nRet = 0;
	if ( mysql_real_query( pConn->m_pMysql, szSql, nSize ) )
	{
		NDLOG_ERROR << " NDMysqlBaseOp::updateSql query failed! " << NDLOG_END;
		NDLOG_ERROR << " mysql query error info:" << mysql_error(pConn->m_pMysql) << NDLOG_END;
	}
	else
	{
		nRet = (NDUint32)mysql_affected_rows(pConn->m_pMysql);
	}
	setIdleHandle( pConn );

	return nRet;
}

NDUint32 NDMysqlBaseOp::insertSql( const char *szSql , NDUint32 nSize, NDBool bChinese )
{
	if ( (NULL == szSql) || ('\0' == szSql[0]) || (0 >= nSize) )
	{
		return 0;
	}

	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return 0;
	}

	if ( NDTrue == bChinese )
	{
		if ( NDFalse == setChineseFont( pConn ) )
		{
			setIdleHandle( pConn );
			return 0;
		}
	}

	NDUint32 nRet = 0;
	if ( mysql_real_query( pConn->m_pMysql, szSql, nSize ) )
	{
		NDLOG_ERROR << " NDMysqlBaseOp::insertSql query failed! " << NDLOG_END;
		NDLOG_ERROR << " mysql query error info:" << mysql_error(pConn->m_pMysql) << NDLOG_END;
	}
	else
	{
		nRet = (NDUint32)mysql_insert_id(pConn->m_pMysql);
	}
	setIdleHandle( pConn );

	return nRet;
}

NDUint32 NDMysqlBaseOp::deleteSql( const char *szSql , NDUint32 nSize, NDBool bChinese )
{
	if ( (NULL == szSql) || ('\0' == szSql[0]) || (0 >= nSize) )
	{
		return 0;
	}

	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return 0;
	}

	if ( NDTrue == bChinese )
	{
		if ( NDFalse == setChineseFont( pConn ) )
		{
			setIdleHandle( pConn );
			return 0;
		}
	}

	NDUint32 nRet = 0;
	if ( mysql_real_query( pConn->m_pMysql, szSql, nSize ) )
	{
		NDLOG_ERROR << " NDMysqlBaseOp::deleteSql query failed! " << NDLOG_END;
		NDLOG_ERROR << " mysql query error info:" << mysql_error(pConn->m_pMysql) << NDLOG_END;
	}
	else
	{
		nRet = (NDUint32)mysql_affected_rows(pConn->m_pMysql);
	}
	setIdleHandle( pConn );

	return nRet;
}


void NDMysqlBaseOp::transBegin()
{
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return ;
	}

	char* pBuf = "START TRANSACTION";
	if ( mysql_real_query( pConn->m_pMysql, pBuf, (unsigned long)strlen(pBuf) ) )
	{
		NDLOG_ERROR << " NDMysqlBaseOp::transBegin query failed! " << NDLOG_END;
		NDLOG_ERROR << " mysql query error info:" << mysql_error(pConn->m_pMysql) << NDLOG_END;
	}
	else
	{
	}
	setIdleHandle( pConn );
}

void NDMysqlBaseOp::transCommit()
{
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return ;
	}

	mysql_commit( pConn->m_pMysql );
	setIdleHandle( pConn );	
}

void NDMysqlBaseOp::transRollback()
{
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return ;
	}

	mysql_rollback( pConn->m_pMysql );
	setIdleHandle( pConn );
}


string NDMysqlBaseOp::getState()
{
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return string("");
	}

	string strState( mysql_stat( pConn->m_pMysql ) );
	setIdleHandle( pConn );	

	return strState;
}

string NDMysqlBaseOp::getServerInfo()
{
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return string("");
	}

	string strServerInfo( mysql_get_server_info( pConn->m_pMysql ) );
	setIdleHandle( pConn );	

	return strServerInfo;
}

string NDMysqlBaseOp::getHostInfo()
{
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return string("");
	}

	string strHostInfo( mysql_get_host_info( pConn->m_pMysql ) );
	setIdleHandle( pConn );	

	return strHostInfo;
}

string NDMysqlBaseOp::getClientInfo()
{
	return string( mysql_get_client_info() );
}

NDUint32 NDMysqlBaseOp::getProtocolInfo()
{
	MysqlConn* pConn = getIdleHandle();
	if ( NULL == pConn)
	{
		return 0;
	}

	NDUint32 nProtoInfo( mysql_get_proto_info( pConn->m_pMysql ) );
	setIdleHandle( pConn );	

	return nProtoInfo;
}


NDBool NDMysqlBaseOp::setChineseFont( MysqlConn* pConn )
{
	NDBool bRet		= NDFalse;
	char* szChinese = "SET NAMES GB2312";
	if ( 0 == mysql_real_query( pConn->m_pMysql, szChinese, (unsigned long)strlen(szChinese) ) )
	{
		bRet = NDTrue;
	}
	else
	{
		NDLOG_ERROR << " [NDMysqlBaseOp::setChineseFont] set chinese font failed! " << NDLOG_END;
	}

	return bRet;
}
















//////////////////////////////////////////////////////////////////////////////////////////////////

NDMysqlQueryResult::NDMysqlQueryResult( MYSQL_RES* pResult, NDUint32 nFieldCount, NDUint32 nRowCount )
: m_pQueryResult( pResult ), m_pCurrentRow( NULL ), m_nFieldCount( nFieldCount ), m_nRowCount( nRowCount )
{
	m_pCurrentRow = new NDField[ nFieldCount ];
}

NDMysqlQueryResult::~NDMysqlQueryResult()
{
	mysql_free_result( m_pQueryResult );
	SAFE_DELETE_ARRAY( m_pCurrentRow );
}

NDBool NDMysqlQueryResult::nextRow()
{
	NDBool	bRet = NDFalse;

	MYSQL_ROW	row = mysql_fetch_row( m_pQueryResult );
	if ( NULL != row )
	{
		for ( NDUint32 i = 0; i < m_nFieldCount; ++i )
		{
			m_pCurrentRow[i].setValue( row[i] );
		}

		bRet = NDTrue;
	}

	return bRet;
}

NDBool NDMysqlQueryResult::ParseSelectResultToStruct( void* pStructAddr, const NDSelectResultType* pSelectResultType, NDUint8 nMemberNum, NDField* pField )
{
	if ( ( NULL == pStructAddr ) || ( NULL == pSelectResultType ) || ( NULL == pField ) )
	{
		NDLOG_ERROR << " [NDMysqlQueryResult::ParseSelectResultToStruct] param is error! " << NDLOG_END;
		return NDFalse;
	}

	NDBool		bValue		= NDFalse;
	NDInt8		int8Value	= 0;
	NDInt16		int16Value	= 0;
	NDInt32		int32Value	= 0;
	NDInt64		int64Value	= 0;
	NDUint8		uint8Value	= 0;
	NDUint16	uint16Value	= 0;
	NDUint32	uint32Value	= 0;
	NDUint64	uint64Value	= 0;
	NDFloat32	floatValue	= 0;
	NDFloat64	doubleValue	= 0;

	NDBool		*pBoolAddr	= NULL;
	NDInt8		*pInt8Addr	= NULL;
	NDInt16		*pInt16Addr	= NULL;
	NDInt32		*pInt32Addr	= NULL;
	NDInt64		*pInt64Addr	= NULL;
	NDUint8		*pUint8Addr	= NULL;
	NDUint16	*pUint16Addr= NULL;
	NDUint32	*pUint32Addr= NULL;
	NDUint64	*pUint64Addr= NULL;
	NDFloat32	*pFloatAddr	= NULL;
	NDFloat64	*pDoubleAddr= NULL;

	const char*	pStrValue	= NULL;
	char*		pCharAddr	= NULL;
	

	for ( NDUint8 i = 0; i < nMemberNum; ++i )
	{
		switch ( pSelectResultType[i].m_nFieldType )
		{
		case FIELD_BOOL:
			{
				bValue		= pField[ pSelectResultType[i].m_nIndex ].getBool();
				pBoolAddr	= (NDBool *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pBoolAddr	= bValue;
			}
			break;
		case FIELD_I8:
			{
				int8Value	= pField[ pSelectResultType[i].m_nIndex ].getInt8();
				pInt8Addr	= (NDInt8 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pInt8Addr	= int8Value;
			}
			break;
		case FIELD_I16:
			{
				int16Value	= pField[ pSelectResultType[i].m_nIndex ].getInt16();
				pInt16Addr	= (NDInt16 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pInt16Addr	= int16Value;
			}
			break;
		case FIELD_I32:
			{
				int32Value	= pField[ pSelectResultType[i].m_nIndex ].getInt32();
				pInt32Addr	= (NDInt32 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pInt32Addr	= int32Value;
			}
			break;
		case FIELD_I64:
			{
				int64Value	= pField[ pSelectResultType[i].m_nIndex ].getInt64();
				pInt64Addr	= (NDInt64 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pInt64Addr	= int64Value;
			}
			break;
		case FIELD_UI8:
			{
				uint8Value	= pField[ pSelectResultType[i].m_nIndex ].getUint8();
				pUint8Addr	= (NDUint8 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pUint8Addr	= uint8Value;
			}
			break;
		case FIELD_UI16:
			{
				uint16Value	= pField[ pSelectResultType[i].m_nIndex ].getUint16();
				pUint16Addr	= (NDUint16 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pUint16Addr= uint16Value;
			}
			break;
		case FIELD_UI32:
			{
				uint32Value	= pField[ pSelectResultType[i].m_nIndex ].getUint32();
				pUint32Addr	= (NDUint32 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pUint32Addr= uint32Value;
			}
			break;
		case FIELD_UI64:
			{
				uint64Value	= pField[ pSelectResultType[i].m_nIndex ].getUint64();
				pUint64Addr	= (NDUint64 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pUint64Addr= uint64Value;
			}
			break;
		case FIELD_FLOAT:
			{
				floatValue	= pField[ pSelectResultType[i].m_nIndex ].getFloat();
				pFloatAddr	= (NDFloat32 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pFloatAddr = floatValue;
			}
			break;
		case FIELD_DOUBLE:
			{
				doubleValue	= pField[ pSelectResultType[i].m_nIndex ].getFloat();
				pDoubleAddr	= (NDFloat64 *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				*pDoubleAddr= doubleValue;
			}
			break;

		case FIELD_STRING:
		case FIELD_BIN_DATA:
			{
				pStrValue	=  pField[ pSelectResultType[i].m_nIndex ].getString();
				pCharAddr	=  (char *)( (NDUint64)pStructAddr + pSelectResultType[i].m_nAddrOffset );
				NDUint32 nStrLen = 0;
				if ( NDTrue == NDShareBaseGlobal::nd_isValidStrParam( pStrValue, nStrLen) )
				{
					NDShareBaseGlobal::nd_safeStrCopy( pCharAddr, pStrValue, pSelectResultType[i].m_nExtend, nStrLen );
				}
			}
			break;
		}
	}

	return NDTrue;
}


_NDSHAREBASE_END


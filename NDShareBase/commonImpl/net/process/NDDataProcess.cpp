#include "net/process/NDDataProcess.h"

#include "NDCLibFun.h"
#include "file/NDConsoleLog.h"

#include "net/stream/NDIStream.h"


_NDSHAREBASE_BEGIN

NDDataProcess::NDDataProcess(void)
{
	m_ProtocolIDNameMap.clear();
	m_CallBackMap.clear();
}

NDDataProcess::~NDDataProcess(void)
{
	m_ProtocolIDNameMap.clear();
	m_CallBackMap.clear();
}

void NDDataProcess::RegisterCallBack( NDUint32 nProtocolID, NDProtocolCallBack *pNDCallBack )
{
#ifdef _DEBUG
	CallBackMapIter iterFind = m_CallBackMap.find( nProtocolID );
	if ( iterFind != m_CallBackMap.end() )
	{
		char szBuf[32] = {0};
		ND_SNPRINTF( szBuf, sizeof(szBuf) - 1, " MessageID: [%u] has registered!", nProtocolID );

		NDLOG_ERROR << szBuf << NDLOG_END;
		return;
	}
#endif

	if ( 0 != pNDCallBack )
	{
		m_CallBackMap.insert( std::make_pair( nProtocolID, pNDCallBack ) );
	}
#ifdef _DEBUG
	else
	{
		NDLOG_ERROR << " [NDDataProcess::RegisterCallBack]  pNDCallBack is NULL! " << NDLOG_END;
	}
#endif
}

NDBool NDDataProcess::Process( NDIStream &rIStream )
{
	NDProtocolHeader protocolHeader;
	rIStream.Read( &protocolHeader, sizeof(NDProtocolHeader) );

	return Process( rIStream, protocolHeader );
}

NDBool NDDataProcess::Process( NDIStream &rIStream, NDProtocolHeader& protocolHeader, NDBool bShowError )
{
	NDBool bRet = NDFalse;

	CallBackMapIter iter = m_CallBackMap.find( protocolHeader.m_nProtocolID );
	if ( iter != m_CallBackMap.end() )
	{
		NDProtocolCallBack *pNDCallBack = iter->second;
		if ( 0 != pNDCallBack )
		{
			bRet = pNDCallBack->Process( rIStream, protocolHeader );
		}
#ifdef _DEBUG	
		else
		{
			ProtocolIDNameMapIter nameIter = m_ProtocolIDNameMap.find( protocolHeader.m_nProtocolID );
			if ( nameIter != m_ProtocolIDNameMap.end() )
			{
				string& strName = nameIter->second;

				char szBuf[64] = {0};
				ND_SNPRINTF( szBuf, sizeof(szBuf) - 1, " MessageName:[%s],MessageID:[%u] has registered, but no register dispose event!", strName.c_str(), protocolHeader.m_nProtocolID );

				NDLOG_ERROR << szBuf << NDLOG_END;
			}
			else
			{
				char szBuf[64] = {0};
				ND_SNPRINTF( szBuf, sizeof(szBuf) - 1, " MessageID:[%u] has registered, but no register dispose event!", protocolHeader.m_nProtocolID );

				NDLOG_ERROR << szBuf << NDLOG_END;
			}
			
		}
#endif
	}
#ifdef _DEBUG
	else
	{
		if ( NDTrue == bShowError )
		{
			ProtocolIDNameMapIter nameIter = m_ProtocolIDNameMap.find( protocolHeader.m_nProtocolID );
			if ( nameIter != m_ProtocolIDNameMap.end() )
			{
				string& strName = nameIter->second;

				char szBuf[64] = {0};
				ND_SNPRINTF( szBuf, sizeof(szBuf) - 1, " MessageName:[%s],MessageID:[%u] has registered, but no register dispose event!", strName.c_str(), protocolHeader.m_nProtocolID );

				NDLOG_ERROR << szBuf << NDLOG_END;
			}
			else
			{
				char szBuf[64] = {0};
				ND_SNPRINTF( szBuf, sizeof(szBuf) - 1, " MessageID:[%u] has registered, but no register dispose event!", protocolHeader.m_nProtocolID );

				NDLOG_ERROR << szBuf << NDLOG_END;
			}
		}
	}
#endif

	return bRet;
}

void NDDataProcess::RegisterProtocolIDName( NDUint32 nProtocolID, string strProtocolIDName )
{
#ifdef _DEBUG
	ProtocolIDNameMapIter iterFind = m_ProtocolIDNameMap.find( nProtocolID );
	if ( iterFind != m_ProtocolIDNameMap.end() )
	{
		string& strName = iterFind->second;

		char szBuf[64] = {0};
		ND_SNPRINTF( szBuf, sizeof(szBuf) - 1, " MessageName:[%s],MessageID:[%u] has registered!", strName.c_str(), nProtocolID );

		NDLOG_ERROR << szBuf << NDLOG_END;

		return;
	}
#endif

	if ( strProtocolIDName.length() > 0 )
	{
		m_ProtocolIDNameMap.insert( std::make_pair( nProtocolID, strProtocolIDName ) );
	}
#ifdef _DEBUG
	else
	{
		NDLOG_ERROR << " Register message name can't be empty." << NDLOG_END;
	}
#endif
}

_NDSHAREBASE_END

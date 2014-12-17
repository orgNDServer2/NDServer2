/********************************************************************
	created:	2014/10/01
	filename: 	d:\code\NDServer2\NDShareBase\common\net\session\NDSessionManager.h
	file base:	NDSessionManager
	purpose:	session manager;
	author:		fanxiangdong;
	mail:		fanxiangdong7@126.com;
	qq:			435337751;
*********************************************************************/
#ifndef __SHARE_BASE_ND_SESSION_MANAGER_H__
#define __SHARE_BASE_ND_SESSION_MANAGER_H__

#include "NDTypes.h"


_NDSHAREBASE_BEGIN

class NDByteBuffer;
class NDSession;
class NDSessionManagerImpl;
class NDProtocol;

class NDSessionManager
{
private:
	NDSessionManagerImpl*		m_pSessionManagerImpl;

	static NDSessionManager*	m_pSessionManager;

public:
	static NDSessionManager*	getInstance();
	static void					releaseInstance();

	~NDSessionManager(void);
	
	NDSession*	createServerSession();
	NDSession*	createClientSession();

	NDBool		addWorkServerSession( NDSession* pServerSession );
	NDBool		addWorkClientSession( NDSession* pClientSession );

	NDSession*  removePrepServerSession( NDUint32 nSessionID );

	NDSession*	findServerSession( NDUint32 nSessionID );
	NDSession*	findClientSession( NDUint32 nSessionID );

	NDBool		sendToServer( NDProtocol& rProtocol, NDUint32 nSessionID, NDUint8 nProDataHeadBitWise );
	NDBool		sendToClient( NDProtocol& rProtocol, NDUint32 nSessionID, NDUint8 nProDataHeadBitWise );

	void		releaseServerSession( NDUint32 nSessionID );
	void		releaseClientSession( NDUint32 nSessionID );

	//update release session to create session map;
	void		updateReleaseSessionMap();
	//update work server session to check invalid session;
	void		updateServerSessionMap();
	//�ر�������Ч��״̬��session;
	void		closeInvalidWorkSession();
	//����sessionΪ��Ч��״̬;
	NDBool		setInvalidWorkSession( SOCKET nSocketID );


	NDBool		eventLoop();

	//�Ż�ע���¼�,����Ҫ���͵�ʱ���ע��WRITE_EVENT;
	NDBool		registerWriteEvent( SOCKET fd );
	NDBool		unregisterWriteEvent( SOCKET fd );


	//����ͨ�ÿͻ��˶Ͽ�����ʱ���ڲ����ϲ��׳���DisconnectNtyProtocol;
	NDBool		setCommonDisconnectNtyProtocol( NDProtocol* pDisconnectNtyProtocol );
	NDBool		popCommonDisconnectNtyProtocol( NDUint32 nSessionID );

	//����Session���յ����ݰ��Ľ�������;
	void		setSessionParsePacketFun( PParsePacketFun parsePacketFun );

	//�Ƿ�Ҫ������Э��;
	//NDBool		isSessionTypeProtocol( NDInt8 sessionProtocolType, NDUint32 nProtocolID );
	//void			setSessionTypeProtocol( NDSessionProtocolType eType, NDUint32 nBeginProtocol, NDUint32 nEndProtocol );

protected:
	NDSessionManager(void);

private:
	NDSessionManager(const NDSessionManager& other);
	NDSessionManager& operator = (const NDSessionManager& other);


};

_NDSHAREBASE_END

#endif
/*
	Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010  Belledonne Communications SARL.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef agent_hh
#define agent_hh


#include <string>

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_tag.h>
#include <sofia-sip/msg.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nta_stateless.h>

#include "common.hh"
#include "configmanager.hh"
#include "module.hh"


/**
 * The agent class represents a SIP agent.
 * It listens on a UDP and TCP port, receives request and responses, 
 * and injects them into the module chain.
 * The module chain is :
 * NatHelper => Authentication => Registrar => ContactRouteInserter => MediaRelay => Transcoder => Forward
 * 
 * Refer to the flexisip.conf.sample installed by "make install" for documentation about what each module does.
**/
class Agent{
	public:
		Agent(su_root_t *root, const char *locaddr, int port, int tlsport);
		virtual void loadConfig(ConfigManager *cm);
		void setDomain(const std::string &domain);
		virtual ~Agent();
		const std::string getLocAddr()const{
			return mLocAddr;
		}
		int getPort()const{
			return mPort;
		}
		/**
		 * return a network unique identifier for this Agent.
		 */
		const std::string& getUniqueId() const;
		void idle();
		bool isUs(const url_t *url)const;
		nta_agent_t* getSofiaAgent()const{
			return mAgent;
		}
		int countUsInVia(sip_via_t *via)const;
		bool isUs(const char *host, const char *port)const;
		const char *getServerString()const;
		typedef void (*timerCallback)(void *unused, su_timer_t *t, void *data);
		su_timer_t *createTimer(int milliseconds, timerCallback cb, void *data);
		void stopTimer(su_timer_t *t);
	protected:
		int onIncomingMessage(msg_t *msg, sip_t *sip);
		void onRequest(msg_t *msg, sip_t *sip);
		void onResponse(msg_t *msg, sip_t *sip);
	private:
		std::string mServerString;
		std::list<Module*> mModules;
		std::list<std::string> mAliases;
		const std::string mLocAddr;
		std::string mDomain;
		const int mPort;
		const int mTlsPort;
		std::string mUniqueId;
		nta_agent_t *mAgent;
		su_root_t *mRoot;
		static int messageCallback(nta_agent_magic_t *context, nta_agent_t *agent,msg_t *msg,sip_t *sip);
};

#endif


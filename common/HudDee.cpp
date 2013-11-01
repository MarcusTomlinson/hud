/*
 * HudDee.cpp
 *
 *  Created on: 1 Nov 2013
 *      Author: pete
 */

#include <common/HudDee.h>

#include <dee.h>
#include <glib-object.h>

using namespace std;
using namespace hud::common;

class HudDee::Priv {
public:

	Priv() :
			m_model(nullptr) {
	}

	~Priv() {
		g_clear_object(&m_model);
	}

	DeeModel *m_model;

	string m_name;
};

HudDee::HudDee(const string &name) :
		p(new Priv()) {

	p->m_name = name;
	p->m_model = dee_shared_model_new(p->m_name.data());
}

HudDee::~HudDee() {
}

const string & HudDee::name() const {
	return p->m_name;
}

void HudDee::setSchema(const char* const *columnSchemas,
		unsigned int numColumns) {
	dee_model_set_schema_full(p->m_model, columnSchemas, numColumns);
}

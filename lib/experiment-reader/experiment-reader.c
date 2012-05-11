/**
 * @file
 * Auxiliary class to handle "session" XML files (augmented Folker).
 * It is a GObject that must be freed using \e g_object_unref.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "experiment-reader.h"

static void experiment_reader_class_init(ExperimentReaderClass *klass);
static void experiment_reader_init(ExperimentReader *klass);
static void experiment_reader_finalize(GObject *gobject);

static gint64 get_timepoint_by_ref(xmlDoc *doc, xmlChar *ref);
static gboolean generic_foreach_topic(xmlDoc *doc, xmlNodeSet *nodes,
				      ExperimentReaderTopicCallback callback,
				      gpointer data);

/** @private */
#define EXPERIMENT_READER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), EXPERIMENT_TYPE_READER, ExperimentReaderPrivate))

/** @private */
struct _ExperimentReaderPrivate {
	xmlDoc *doc;
};

/** @private */
GType
experiment_reader_get_type(void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info = {
			sizeof(ExperimentReaderClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)experiment_reader_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(ExperimentReader),
			0,    /* n_preallocs */
			(GInstanceInitFunc)experiment_reader_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
					      "ExperimentReader", &info, 0);
	}

	return type;
}

static void
experiment_reader_class_init(ExperimentReaderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	/* gobject_class->dispose = experiment_reader_dispose; */
	gobject_class->finalize = experiment_reader_finalize;

	g_type_class_add_private(klass, sizeof(ExperimentReaderPrivate));
}

static void
experiment_reader_init(ExperimentReader *klass)
{
	klass->priv = EXPERIMENT_READER_GET_PRIVATE(klass);

	klass->priv->doc = NULL;
}

static void
experiment_reader_finalize(GObject *gobject)
{
	ExperimentReader *reader = EXPERIMENT_READER(gobject);

	if (reader->priv->doc != NULL)
		xmlFreeDoc(reader->priv->doc);

	/* Chain up to the parent class */
	//G_OBJECT_CLASS(experiment_reader_parent_class)->finalize(gobject);
}

static gint64
get_timepoint_by_ref(xmlDoc *doc, xmlChar *ref)
{
	xmlChar expr[255];

	xmlXPathContext	*xpathCtx;
	xmlXPathObject	*xpathObj;

	double value;

	xpathCtx = xmlXPathNewContext(doc);
	assert(xpathCtx != NULL);

	xmlStrPrintf(expr, sizeof(expr),
		     (const xmlChar *)"/session/timeline/"
				      "timepoint[@timepoint-id = '%s']/"
				      "@absolute-time", ref);

	xpathObj = xmlXPathEvalExpression(expr, xpathCtx);
	assert(xpathObj != NULL);

	value = xmlXPathCastToNumber(xpathObj);

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);

	return (gint64)(value*1000.);
}

static gboolean
generic_foreach_topic(xmlDoc *doc, xmlNodeSet *nodes,
		      ExperimentReaderTopicCallback callback, gpointer data)
{
	if (nodes == NULL)
		return TRUE;

	for (int i = 0; i < nodes->nodeNr; i++) {
		xmlNode *cur = nodes->nodeTab[i];
		xmlNode *contrib = cur->children;
		assert(cur != NULL && cur->type == XML_ELEMENT_NODE);

		xmlChar	*topic_id = xmlGetProp(cur, (const xmlChar *)"id");
		gint64	start_time = -1;

		if (contrib != NULL) {
			xmlChar *contrib_start_ref;

			contrib_start_ref = xmlGetProp(contrib, (const xmlChar *)"start-reference");
			start_time = get_timepoint_by_ref(doc, contrib_start_ref);
			xmlFree(contrib_start_ref);
		}

		callback((const gchar *)topic_id, start_time, data);

		xmlFree(topic_id);
	}

	return FALSE;
}

/*
 * API
 */

/**
 * Constructs a new ExperimentReader object.
 *
 * @param filename Filename of XML file to open
 * @return A new \e ExperimentReader object. Free with \e g_object_unref.
 */
ExperimentReader *
experiment_reader_new(const gchar *filename)
{
	ExperimentReader *reader;

	reader = EXPERIMENT_READER(g_object_new(EXPERIMENT_TYPE_READER, NULL));
	reader->priv->doc = xmlParseFile(filename);
	if (reader->priv->doc == NULL) {
		g_object_unref(G_OBJECT(reader));
		return NULL;
	}

	/** @todo validate against session.dtd */

	return reader;
}

/**
 * Calls \e callback with \e userdata for each \b topic in the \b greetings
 * section of the experiment.
 *
 * @param reader   \e ExperimentReader instance
 * @param callback Function to invoke
 * @param userdata User data to pass to \e callback
 */
void
experiment_reader_foreach_greeting_topic(ExperimentReader *reader,
					 ExperimentReaderTopicCallback callback,
					 gpointer userdata)
{
	xmlXPathContext	*xpathCtx;
	xmlXPathObject	*xpathObj;

	xpathCtx = xmlXPathNewContext(reader->priv->doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *)"/session/greeting/topic",
					  xpathCtx);

	generic_foreach_topic(reader->priv->doc, xpathObj->nodesetval,
			      callback, userdata);

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);
}

/**
 * Calls \e callback with \e userdata for each \b topic in the
 * \b initial-narrative subsection of the \b experiment section of
 * the experiment.
 *
 * @param reader   \e ExperimentReader instance
 * @param callback Function to invoke
 * @param userdata User data to pass to \e callback
 */
void
experiment_reader_foreach_exp_initial_narrative_topic(reader, callback, userdata)
	ExperimentReader		*reader;
	ExperimentReaderTopicCallback	callback;
	gpointer			userdata;
{
	xmlXPathContext	*xpathCtx;
	xmlXPathObject	*xpathObj;

	xpathCtx = xmlXPathNewContext(reader->priv->doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *)"/session/experiment/"
							   "initial-narrative/topic",
					  xpathCtx);

	generic_foreach_topic(reader->priv->doc, xpathObj->nodesetval,
			      callback, userdata);

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);
}

/**
 * Calls \e callback with \e userdata for each \b topic in a \b phase of
 * the \b last-minute subsection of the \b experiment section of
 * the experiment.
 *
 * @param reader   \e ExperimentReader instance
 * @param phase    \b Phase section (integer between 1 and 6)
 * @param callback Function to invoke
 * @param userdata User data to pass to \e callback
 */
void
experiment_reader_foreach_exp_last_minute_phase_topic(reader, phase, callback, userdata)
	ExperimentReader		*reader;
	gint				phase;
	ExperimentReaderTopicCallback	callback;
	gpointer			userdata;
{
	xmlXPathContext	*xpathCtx;
	xmlXPathObject	*xpathObj;

	xmlChar expr[255];

	xpathCtx = xmlXPathNewContext(reader->priv->doc);

	/* Evaluate xpath expression */
	xmlStrPrintf(expr, sizeof(expr),
		     (const xmlChar *)"/session/experiment/last-minute/"
		     		      "phase[%d]/topic",
		     phase);
	xpathObj = xmlXPathEvalExpression(expr, xpathCtx);

	generic_foreach_topic(reader->priv->doc, xpathObj->nodesetval,
			      callback, userdata);

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);
}

/**
 * Calls \e callback with \e userdata for each \b topic in the \b farewell
 * section of the experiment.
 *
 * @param reader   \e ExperimentReader instance
 * @param callback Function to invoke
 * @param userdata User data to pass to \e callback
 */
void
experiment_reader_foreach_farewell_topic(ExperimentReader *reader,
					 ExperimentReaderTopicCallback callback,
					 gpointer userdata)
{
	xmlXPathContext	*xpathCtx;
	xmlXPathObject	*xpathObj;

	xpathCtx = xmlXPathNewContext(reader->priv->doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *)"/session/farewell/topic",
					  xpathCtx);

	generic_foreach_topic(reader->priv->doc, xpathObj->nodesetval,
			      callback, userdata);

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);
}
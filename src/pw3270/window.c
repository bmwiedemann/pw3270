/*
 * "Software pw3270, desenvolvido com base nos códigos fontes do WC3270  e X3270
 * (Paul Mattes Paul.Mattes@usa.net), de emulação de terminal 3270 para acesso a
 * aplicativos mainframe. Registro no INPI sob o nome G3270.
 *
 * Copyright (C) <2008> <Banco do Brasil S.A.>
 *
 * Este programa é software livre. Você pode redistribuí-lo e/ou modificá-lo sob
 * os termos da GPL v.2 - Licença Pública Geral  GNU,  conforme  publicado  pela
 * Free Software Foundation.
 *
 * Este programa é distribuído na expectativa de  ser  útil,  mas  SEM  QUALQUER
 * GARANTIA; sem mesmo a garantia implícita de COMERCIALIZAÇÃO ou  de  ADEQUAÇÃO
 * A QUALQUER PROPÓSITO EM PARTICULAR. Consulte a Licença Pública Geral GNU para
 * obter mais detalhes.
 *
 * Você deve ter recebido uma cópia da Licença Pública Geral GNU junto com este
 * programa; se não, escreva para a Free Software Foundation, Inc., 51 Franklin
 * St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Este programa está nomeado como window.c e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 * licinio@bb.com.br		(Licínio Luis Branco)
 * kraucer@bb.com.br		(Kraucer Fernandes Mazuco)
 *
 */

#include "private.h"
#include "uiparser/parser.h"
#include <lib3270/popup.h>
#include <lib3270/actions.h>
#include <lib3270/trace.h>
#include <lib3270/log.h>
#include <lib3270/toggle.h>
#include <lib3270/properties.h>
#include <v3270/trace.h>
#include <v3270/toggle.h>
#include <v3270/settings.h>
#include "common/common.h"

/*--[ Widget definition ]----------------------------------------------------------------------------*/

 struct _pw3270
 {
	GtkWindow		  parent;
 	GtkWidget		* terminal;
 };

 struct _pw3270Class
 {
	GtkWindowClass parent_class;

	int dummy;
 };

 G_DEFINE_TYPE(pw3270, pw3270, GTK_TYPE_WINDOW);

/*--[ Globals ]--------------------------------------------------------------------------------------*/

 enum action_group
 {
	ACTION_GROUP_DEFAULT,
	ACTION_GROUP_ONLINE,
	ACTION_GROUP_OFFLINE,
	ACTION_GROUP_SELECTION,
	ACTION_GROUP_CLIPBOARD,
	ACTION_GROUP_FILETRANSFER,
	ACTION_GROUP_PASTE,

	ACTION_GROUP_MAX
 };

 enum popup_group
 {
 	POPUP_DEFAULT,
 	POPUP_ONLINE,
 	POPUP_OFFLINE,
 	POPUP_SELECTION,

 	POPUP_MAX
 };

 static const gchar *groupname[ACTION_GROUP_MAX+1] = {	"default",
														"online",
														"offline",
														"selection",
														"clipboard",
														"filetransfer",
														"paste",
														NULL
														};

 static const gchar *popupname[POPUP_MAX+1] = {			"default",
														"online",
														"offline",
														"selection",
														NULL
														};

static GtkWidget * trace_window = NULL;

 const gchar			* oversize		= NULL;

/*--[ Implement ]------------------------------------------------------------------------------------*/

#if GTK_CHECK_VERSION(3,0,0)
 static void pw3270_destroy(GtkWidget *widget)
#else
 static void pw3270_destroy(GtkObject *widget)
#endif
 {
	pw3270 * window = GTK_PW3270(widget);

	trace("%s %p",__FUNCTION__,widget);

 	if(window->terminal)
		v3270_disconnect(window->terminal);

 }

 static gboolean window_state_event(GtkWidget *window, GdkEventWindowState *event, GtkWidget *widget)
 {
	gboolean	  fullscreen	= event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN ? TRUE : FALSE;
	GtkAction	**action		= (GtkAction **) g_object_get_data(G_OBJECT(widget),"named_actions");

	// Update fullscreen toggles
	if(action[ACTION_FULLSCREEN])
		gtk_action_set_visible(action[ACTION_FULLSCREEN],!fullscreen);

	if(action[ACTION_UNFULLSCREEN])
		gtk_action_set_visible(action[ACTION_UNFULLSCREEN],fullscreen);

	lib3270_set_toggle(v3270_get_session(widget),LIB3270_TOGGLE_FULL_SCREEN,fullscreen);

	save_window_state_to_config("window", "toplevel", event->new_window_state);

	return 0;
 }

 static gboolean configure_event(GtkWidget *widget, GdkEvent  *event, gpointer   user_data)
 {
 	GdkWindowState CurrentState = gdk_window_get_state(gtk_widget_get_window(widget));

	if( !(CurrentState & (GDK_WINDOW_STATE_FULLSCREEN|GDK_WINDOW_STATE_MAXIMIZED|GDK_WINDOW_STATE_ICONIFIED)) )
		save_window_size_to_config("window","toplevel",widget);

	return 0;
 }

  static void pw3270_class_init(pw3270Class *klass)
 {
#if GTK_CHECK_VERSION(3,0,0)
	GtkWidgetClass	* widget_class	= GTK_WIDGET_CLASS(klass);
	widget_class->destroy = pw3270_destroy;
#else
	{
		GtkObjectClass *object_class = (GtkObjectClass*) klass;
		object_class->destroy = pw3270_destroy;
	}
#endif // GTK3

	configuration_init();

 }

 static void trace_on_file(G_GNUC_UNUSED H3270 *hSession, void * userdata, const char *fmt, va_list args)
 {
	int err;

	FILE *out = fopen(tracefile,"a");
	err = errno;

	if(!out)
	{
		// Error opening trace file, notify user and disable it
		GtkWidget *popup = gtk_message_dialog_new_with_markup(
											GTK_WINDOW(userdata),
											GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
											GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
											_( "Can't save trace data to file %s" ),tracefile);

		gtk_window_set_title(GTK_WINDOW(popup),_("Can't open file"));

		gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(popup),"%s",strerror(err));

		gtk_dialog_run(GTK_DIALOG(popup));
		gtk_widget_destroy(popup);

		tracefile = NULL;
	}
	else
	{
		vfprintf(out,fmt,args);
		fclose(out);
	}

 }

 static void trace_window_destroy(GtkWidget *widget, H3270 *hSession)
 {
	trace("%s",__FUNCTION__);
	lib3270_set_toggle(hSession,LIB3270_TOGGLE_DS_TRACE,0);
	lib3270_set_toggle(hSession,LIB3270_TOGGLE_SCREEN_TRACE,0);
	lib3270_set_toggle(hSession,LIB3270_TOGGLE_EVENT_TRACE,0);
	lib3270_set_toggle(hSession,LIB3270_TOGGLE_NETWORK_TRACE,0);

	trace_window = NULL;

 }

 struct trace_data
 {
	H3270	* hSession;
	gchar	* text;
 };

 static gboolean trace_window_delete_event(GtkWidget *widget, GdkEvent  *event, gpointer user_data)
 {
	return FALSE;
 }

 static gboolean bg_trace_window(struct trace_data *data)
 {
 	if(!trace_window)
	{
		trace_window = v3270_trace_window_new(lib3270_get_user_data(data->hSession),data->text);

		if(trace_window)
		{
			g_signal_connect(trace_window, "destroy", G_CALLBACK(trace_window_destroy), data->hSession);
			g_signal_connect(trace_window, "delete-event", G_CALLBACK(trace_window_delete_event), data->hSession);
		}

	}

	if(trace_window)
		gtk_widget_show_all(trace_window);

	g_free(data->text);

	return FALSE;
 }

 static void trace_on_window(G_GNUC_UNUSED H3270 *hSession, G_GNUC_UNUSED void * userdata, const char *fmt, va_list args)
 {
	struct trace_data * data = g_new0(struct trace_data,1);

	data->hSession = hSession;
	data->text = g_strdup_vprintf(fmt,args);

	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,(GSourceFunc) bg_trace_window, data, g_free);

 }

 static gboolean bg_auto_connect(GtkWidget *widget)
 {
	pw3270_connect(widget);
	return FALSE;
 }

 GtkWidget * pw3270_new(const gchar *host, const gchar *systype, unsigned short colors)
 {
 	GtkWidget	* widget	= g_object_new(GTK_TYPE_PW3270, NULL);
 	gboolean	  connct	= FALSE;

	v3270_set_dynamic_font_spacing(GTK_PW3270(widget)->terminal,get_boolean_from_config("toggle","dspacing",FALSE));

	if(tracefile)
	{
		lib3270_set_trace_handler(pw3270_get_session(widget),trace_on_file,(void *) widget);
	}
	else
	{
		lib3270_set_trace_handler(pw3270_get_session(widget),trace_on_window,(void *) widget);
	}

	if(host)
	{
		set_string_to_config("host","uri","%s",host);
		pw3270_set_url(widget,host);
	}
	else
	{
		gchar *ptr = get_string_from_config("host","uri","");
		pw3270_set_url(widget,ptr);
		g_free(ptr);
	}

	if(systype)
	{
        set_string_to_config("host","systype","%s",systype);
		pw3270_set_host_type(widget,systype);
	}
	else
	{
		gchar *ptr = get_string_from_config("host","systype","S390");
		pw3270_set_host_type(widget,ptr);
		g_free(ptr);
	}

	if(colors)
		set_integer_to_config("host","colortype",colors);
	else
		colors = get_integer_from_config("host","colortype",16);

	pw3270_set_session_color_type(widget,colors);

	v3270_set_scaled_fonts(GTK_PW3270(widget)->terminal,get_boolean_from_config("terminal","sfonts",FALSE));

	if(pw3270_get_toggle(widget,LIB3270_TOGGLE_CONNECT_ON_STARTUP))
		g_idle_add((GSourceFunc) bg_auto_connect, widget);

 	return widget;
 }

 void pw3270_connect(GtkWidget *widget)
 {
 	g_return_if_fail(GTK_IS_PW3270(widget));
 	v3270_reconnect(GTK_PW3270(widget)->terminal);
 }

 void pw3270_set_url(GtkWidget *widget, const gchar *uri)
 {
 	g_return_if_fail(GTK_IS_PW3270(widget));
 	v3270_set_url(GTK_PW3270(widget)->terminal,uri);
 }

 const gchar * pw3270_get_url(GtkWidget *widget)
 {
 	g_return_val_if_fail(GTK_IS_PW3270(widget),"");
 	return v3270_get_url(GTK_PW3270(widget)->terminal);
 }

 gboolean pw3270_get_toggle(GtkWidget *widget, LIB3270_TOGGLE_ID ix)
 {
 	g_return_val_if_fail(GTK_IS_PW3270(widget),FALSE);
 	return v3270_get_toggle(GTK_PW3270(widget)->terminal,ix);
 }

 H3270 * pw3270_get_session(GtkWidget *widget)
 {
 	g_return_val_if_fail(GTK_IS_PW3270(widget),NULL);
 	return v3270_get_session(GTK_PW3270(widget)->terminal);
 }

 const gchar * pw3270_get_session_name(GtkWidget *widget)
 {
 	if(widget && GTK_IS_PW3270(widget))
		return v3270_get_session_name(GTK_PW3270(widget)->terminal);
	return g_get_application_name();
 }

 static void update_window_title(GtkWidget *window)
 {
	GtkWidget			* widget	= GTK_PW3270(window)->terminal;
	const gchar			* url		= v3270_get_url(widget);
	g_autofree gchar	* title		= NULL;

	if(url)
	{
		g_autofree gchar * base = g_strdup(url);
		gchar *hostname = strstr(base,"://");
		if(hostname)
		{
			gchar *ptr;

			hostname += 3;

			ptr = strchr(hostname,':');
			if(ptr)
				*ptr = 0;

			ptr = strchr(hostname,'?');
			if(ptr)
				*ptr = 0;

		}

		if(hostname && *hostname)
			title = g_strdup_printf("%s - %s",v3270_get_session_name(widget),hostname);
		else
			title = g_strdup_printf("%s",v3270_get_session_name(widget));

	}
	else if(v3270_is_connected(widget))
	{
		title = g_strdup_printf("%s - disconnected",v3270_get_session_name(widget));
	}
	else
	{
		title = g_strdup(v3270_get_session_name(widget));
	}

	gtk_window_set_title(GTK_WINDOW(window),title);

 }

 static void session_changed(GtkWidget *widget, GtkWidget *window)
 {
	update_window_title(window);
 }

 static void save_terminal_settings(GtkWidget *widget, GtkWidget *window)
 {
 	debug("%s",__FUNCTION__);

#ifdef ENABLE_WINDOWS_REGISTRY

	HKEY hKey;

	if(get_registry_handle(NULL, &hKey, KEY_SET_VALUE))
	{
		v3270_to_registry(widget, hKey, "terminal");
		RegCloseKey(hKey);
	}

#else

	v3270_to_key_file(widget, get_application_keyfile(), "terminal");

#endif // _WIN32

 }

 LIB3270_EXPORT void pw3270_set_session_name(GtkWidget *widget, const gchar *name)
 {
 	g_return_if_fail(GTK_IS_PW3270(widget));
	v3270_set_session_name(GTK_PW3270(widget)->terminal,name);
 }

 LIB3270_EXPORT void pw3270_set_oversize(GtkWidget *widget, const gchar *oversize)
 {
 	g_return_if_fail(GTK_IS_PW3270(widget));
	lib3270_set_oversize(pw3270_get_session(widget),oversize);
 }

 LIB3270_EXPORT const gchar * pw3270_get_oversize(GtkWidget *widget)
 {
 	g_return_val_if_fail(GTK_IS_PW3270(widget),"");
 	return (const gchar *) lib3270_get_oversize(pw3270_get_session(widget));
 }

 LIB3270_EXPORT void pw3270_set_host_type(GtkWidget *widget, const gchar *name)
 {
 	size_t f;
 	size_t sz;

 	g_return_if_fail(GTK_IS_PW3270(widget));

	int rc = v3270_set_host_type_by_name(GTK_PW3270(widget)->terminal,name);

	if(!rc) {
		return;
	}

	GtkWidget *popup = gtk_message_dialog_new_with_markup(
						GTK_WINDOW(widget),
						GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
						_( "Can't recognize \"%s\" as a valid host type" ), name);

	gtk_window_set_title(GTK_WINDOW(popup),strerror(rc));

	// Obtenho as opções válidas.
	char text[4096];
	const LIB3270_HOST_TYPE_ENTRY * host_types = lib3270_get_option_list();

	*text = 0;
	for(f=1;host_types[f].name;f++)
	{
		sz = strlen(text);
		snprintf(text+sz,4095-sz,_( "%s<b>%s</b> for %s"), *text ? ", " : "",host_types[f].name,gettext(host_types[f].description));

	}

	sz = strlen(text);
	snprintf(text+sz,4095-sz,_( " and <b>%s</b> for %s."),host_types[0].name,gettext(host_types[0].description));

	gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(popup),_( "The known types are %s" ),text);


	g_signal_connect(popup,"close",G_CALLBACK(gtk_widget_destroy),NULL);
	g_signal_connect(popup,"response",G_CALLBACK(gtk_widget_destroy),NULL);
	gtk_widget_show_all(popup);

	/*
	gtk_dialog_run(GTK_DIALOG(popup));
	gtk_widget_destroy(popup);
	*/

 }

 LIB3270_EXPORT int pw3270_set_session_color_type(GtkWidget *widget, unsigned short colortype)
 {
 	g_return_val_if_fail(GTK_IS_PW3270(widget),EFAULT);
	return v3270_set_session_color_type(GTK_PW3270(widget)->terminal,colortype);
 }

 static void chktoplevel(GtkWidget *window, GtkWidget **widget)
 {
 	if(*widget)
		return;

	if(GTK_IS_PW3270(window))
		*widget = window;
 }

 LIB3270_EXPORT GtkWidget * pw3270_get_toplevel(void)
 {
	GtkWidget	* widget	= NULL;
	GList		* lst 		= gtk_window_list_toplevels();

	g_list_foreach(lst, (GFunc) chktoplevel, &widget);

	g_list_free(lst);
	return widget;
 }

 LIB3270_EXPORT GtkWidget * pw3270_get_terminal_widget(GtkWidget *widget)
 {
  	g_return_val_if_fail(GTK_IS_PW3270(widget),NULL);
 	return GTK_PW3270(widget)->terminal;
 }

 static void setup_input_method(GtkWidget *widget, GtkWidget *obj)
 {
	GtkWidget *menu	= gtk_menu_new();
	gtk_im_multicontext_append_menuitems((GtkIMMulticontext *) v3270_get_im_context(obj) ,GTK_MENU_SHELL(menu));
	gtk_widget_show_all(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget),menu);
 }

 static void set_screen_size(GtkCheckMenuItem *item, GtkWidget *widget)
 {
	if(gtk_check_menu_item_get_active(item))
	{
		char name[2];
		int  model = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),"mode_3270"));

		if(model == lib3270_get_model_number(v3270_get_session(widget)))
			return;

		trace("screen model on widget %p changes to %d",widget,GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),"mode_3270")));

		name[0] = model+'0';
		name[1] = 0;
		lib3270_set_model(v3270_get_session(widget),name);
	}
 }

 static void update_model(GtkWidget *widget, guint id, const gchar *name, GtkWidget **radio)
 {
 	int f;

 	trace("Widget %p changed to %s (id=%d)",widget,name,id);
	set_integer_to_config("terminal","model_number",id);
 	set_string_to_config("terminal","model_name","%s",name);

	id -= 2;
	for(f=0;radio[f];f++)
	{
		if(f == id)
		{
			if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(radio[f])))
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(radio[f]),TRUE);
			return;
		}
	}
 }

 static void setup_screen_sizes(GtkWidget *widget, GtkWidget *obj)
 {
 	static const gchar 	* text[]	= { "80x24", "80x32", "80x43", "132x27" };
	GtkWidget			* menu		= gtk_menu_new();
 	int			  		  model		= lib3270_get_model_number(v3270_get_session(obj))-2;
	GSList 				* group		= NULL;
 	int					  f;
 	GtkWidget 			**item		= g_new0(GtkWidget *,G_N_ELEMENTS(text)+1);

	gtk_widget_set_sensitive(widget,TRUE);

	for(f=0;f<G_N_ELEMENTS(text);f++)
	{
		gchar 		* name = g_strdup_printf( _( "Model %d (%s)"),f+2,text[f]);

		item[f] = gtk_radio_menu_item_new_with_label(group,name);
		g_free(name);

		g_object_set_data(G_OBJECT(item[f]),"mode_3270",GINT_TO_POINTER((f+2)));

		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item[f]));

		gtk_widget_show(item[f]);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),item[f]);

		if(f == model)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item[f]),TRUE);

		g_signal_connect(G_OBJECT(item[f]),"toggled",G_CALLBACK(set_screen_size),(gpointer) obj);

	}
	g_object_set_data_full(G_OBJECT(menu),"screen_sizes",item,g_free);
	g_signal_connect(obj,"model_changed",G_CALLBACK(update_model),item);

	gtk_widget_show_all(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget),menu);
 }

 static void pastenext(GtkWidget *widget, gboolean on, GtkAction **action)
 {
 	pw3270_set_action_state(action[ACTION_PASTENEXT],on);
 }

 static void disconnected(GtkWidget *terminal, GtkWidget * window)
 {
	GtkActionGroup	**group		= g_object_get_data(G_OBJECT(window),"action_groups");
	GtkWidget		**keypad	= g_object_get_data(G_OBJECT(window),"keypads");

	if(group)
	{
		gtk_action_group_set_sensitive(group[ACTION_GROUP_PASTE],FALSE);
		gtk_action_group_set_sensitive(group[ACTION_GROUP_ONLINE],FALSE);
		gtk_action_group_set_sensitive(group[ACTION_GROUP_OFFLINE],TRUE);
	}

	if(keypad)
	{
		while(*keypad)
			gtk_widget_set_sensitive(*(keypad++),FALSE);
	}

	update_window_title(window);
 }

 static void connected(GtkWidget *terminal, const gchar *host, GtkWidget * window)
 {
	GtkActionGroup	**group		= g_object_get_data(G_OBJECT(window),"action_groups");
	GtkWidget		**keypad	= g_object_get_data(G_OBJECT(window),"keypads");

	trace("%s(%s)",__FUNCTION__,host ? host : "NULL");

	if(group)
	{
		gtk_action_group_set_sensitive(group[ACTION_GROUP_ONLINE],TRUE);
		gtk_action_group_set_sensitive(group[ACTION_GROUP_OFFLINE],FALSE);
		gtk_action_group_set_sensitive(group[ACTION_GROUP_PASTE],TRUE);
	}

	if(keypad)
	{
		while(*keypad)
			gtk_widget_set_sensitive(*(keypad++),TRUE);
	}

	set_string_to_config("host","uri","%s",host);

	update_window_title(window);

 }

 static void update_config(GtkWidget *widget, const gchar *name, const gchar *value)
 {
 	set_string_to_config("terminal",name,"%s",value);
 }

 static void selecting(GtkWidget *widget, gboolean on, GtkActionGroup **group)
 {
	GtkAction **action = (GtkAction **) g_object_get_data(G_OBJECT(widget),"named_actions");
	gtk_action_group_set_sensitive(group[ACTION_GROUP_SELECTION],on);
	pw3270_set_action_state(action[ACTION_RESELECT],!on);
 }

 static gboolean popup_menu(GtkWidget *widget, gboolean selected, gboolean online, GdkEventButton *event, GtkWidget **popup)
 {
 	GtkWidget *menu = NULL;

	if(!online)
		menu = popup[POPUP_OFFLINE];
	else if(selected && popup[POPUP_SELECTION])
		menu = popup[POPUP_SELECTION];
	else if(popup[POPUP_ONLINE])
		menu = popup[POPUP_ONLINE];
	else
		menu = popup[POPUP_DEFAULT];

 	trace("Popup %p on widget %p online=%s selected=%s",menu,widget,online ? "Yes" : "No", selected ? "Yes" : "No");

	if(!menu)
		return FALSE;

	trace("Showing popup \"%s\"",gtk_widget_get_name(menu));

	gtk_widget_show_all(menu);
	gtk_menu_set_screen(GTK_MENU(menu), gtk_widget_get_screen(widget));
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button,event->time);

 	return TRUE;
 }

 static void has_text(GtkWidget *widget, gboolean on, GtkActionGroup **group)
 {
	gtk_action_group_set_sensitive(group[ACTION_GROUP_CLIPBOARD],on);
 }

 static void print_all(GtkWidget *widget, GtkWidget *window)
 {
	pw3270_print(widget, NULL, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, LIB3270_CONTENT_ALL);
 }

 static void toggle_changed(GtkWidget *widget, LIB3270_TOGGLE_ID id, gboolean toggled, const gchar *name, GtkWindow *toplevel)
 {
	GtkAction **list = (GtkAction **) g_object_get_data(G_OBJECT(widget),"toggle_actions");
 	gchar *nm = g_ascii_strdown(name,-1);

	set_boolean_to_config("toggle",nm,toggled);
	g_free(nm);

	if(list[id])
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(list[id]),toggled);

 }

 static gboolean field_clicked(GtkWidget *widget, gboolean connected, V3270_OIA_FIELD field, GdkEventButton *event, GtkWidget *window)
 {
	trace("%s: %s field=%d event=%p window=%p",__FUNCTION__,connected ? "Connected" : "Disconnected", field, event, window);

	if(!connected)
		return FALSE;

	if(field == V3270_OIA_SSL)
	{
		v3270_popup_security_dialog(widget);
		trace("%s: Show SSL connection info dialog",__FUNCTION__);
		return TRUE;
	}


	return FALSE;
 }

 static void pw3270_init(pw3270 *widget)
 {
 	static const UI_WIDGET_SETUP widget_setup[] =
 	{
 		{ "inputmethod",	setup_input_method	},
 		{ "screensizes",	setup_screen_sizes	},
 		{ "fontselect",		setup_font_list		},
 		{ NULL,				NULL				}
 	};

 	/*
 	static const struct _widget_config
 	{
 		const gchar *key;
 		void 		(*set)(GtkWidget *widget, const gchar *str);
 	} widget_config[] =
 	{
 		{ "colors", 		v3270_set_colors		},
 		{ "font-family",	v3270_set_font_family	}
 	};
 	*/

	int f;
	GtkAction	**action = g_new0(GtkAction *,ACTION_COUNT);
	H3270 		* host;

	trace("%s(%p)",__FUNCTION__,widget);

	// Initialize terminal widget
	widget->terminal = v3270_new();
	host = v3270_get_session(widget->terminal);

	// Load terminal settings before connecting the signals.
	load_terminal_settings(widget->terminal);

	g_object_set_data_full(G_OBJECT(widget->terminal),"toggle_actions",g_new0(GtkAction *,LIB3270_TOGGLE_COUNT),g_free);
	g_object_set_data_full(G_OBJECT(widget->terminal),"named_actions",(gpointer) action, (GDestroyNotify) g_free);

	// Load UI
	{
#ifdef DEBUG
		const char *path = "ui";
#else
		lib3270_autoptr(char) path = lib3270_build_data_filename("ui",NULL);
#endif // DEBUG

		trace("Loading UI from \"%s\"",path);
		trace("Current dir is \"%s\"",g_get_current_dir());

		if(ui_parse_xml_folder(GTK_WINDOW(widget),path,groupname,popupname,widget->terminal,widget_setup))
		{
			gtk_widget_set_sensitive(widget->terminal,FALSE);
			return;
		}

	}


	// Setup actions
	{
		GtkWidget		**popup = g_object_get_data(G_OBJECT(widget),"popup_menus");
		GtkActionGroup	**group	= g_object_get_data(G_OBJECT(widget),"action_groups");

		// Setup action groups
		gtk_action_group_set_sensitive(group[ACTION_GROUP_SELECTION],FALSE);
		gtk_action_group_set_sensitive(group[ACTION_GROUP_CLIPBOARD],FALSE);
		gtk_action_group_set_sensitive(group[ACTION_GROUP_FILETRANSFER],FALSE);
		gtk_action_group_set_sensitive(group[ACTION_GROUP_PASTE],FALSE);

		disconnected(widget->terminal, GTK_WIDGET(widget));

		// Setup actions
		if(action[ACTION_FULLSCREEN])
			gtk_action_set_visible(action[ACTION_FULLSCREEN],!lib3270_get_toggle(host,LIB3270_TOGGLE_FULL_SCREEN));

		if(action[ACTION_UNFULLSCREEN])
			gtk_action_set_visible(action[ACTION_UNFULLSCREEN],lib3270_get_toggle(host,LIB3270_TOGGLE_FULL_SCREEN));

		if(action[ACTION_PASTENEXT])
		{
			pw3270_set_action_state(action[ACTION_PASTENEXT],FALSE);
			g_signal_connect(widget->terminal,"pastenext",G_CALLBACK(pastenext),action);
		}

		pw3270_set_action_state(action[ACTION_RESELECT],FALSE);

		// Connect action signals
		g_signal_connect(widget->terminal,"disconnected",G_CALLBACK(disconnected),widget);
		g_signal_connect(widget->terminal,"connected",G_CALLBACK(connected),widget);
		g_signal_connect(widget->terminal,"update_config",G_CALLBACK(update_config),0);
		g_signal_connect(widget->terminal,"selecting",G_CALLBACK(selecting),group);
		g_signal_connect(widget->terminal,"popup",G_CALLBACK(popup_menu),popup);
		g_signal_connect(widget->terminal,"has_text",G_CALLBACK(has_text),group);
		g_signal_connect(widget->terminal,"keypress",G_CALLBACK(handle_keypress),NULL);

	}

	// Connect widget signals
	g_signal_connect(widget->terminal,"field_clicked",G_CALLBACK(field_clicked),widget);
	g_signal_connect(widget->terminal,"toggle_changed",G_CALLBACK(toggle_changed),widget);
	g_signal_connect(widget->terminal,"session_changed",G_CALLBACK(session_changed),widget);
	g_signal_connect(widget->terminal,"save_settings",G_CALLBACK(save_terminal_settings),widget);

	//g_signal_connect(widget->terminal,"print",G_CALLBACK(print_all),widget);

	// Connect window signals
	g_signal_connect(widget,"window_state_event",G_CALLBACK(window_state_event),widget->terminal);
	g_signal_connect(widget,"configure_event",G_CALLBACK(configure_event),widget->terminal);

	// Finish setup
#ifdef DEBUG
	lib3270_testpattern(host);
#endif

	trace("%s ends",__FUNCTION__);
	gtk_window_set_focus(GTK_WINDOW(widget),widget->terminal);

 }


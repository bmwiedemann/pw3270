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
 * Este programa está nomeado como - e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */

 /**
  * @brief Implement PW3270 save actions.
  *
  */

 #include "private.h"
 #include <v3270.h>
 #include <v3270/keyfile.h>
 #include <pw3270.h>
 #include <pw3270/application.h>


 static GtkWidget * factory(V3270SimpleAction *action, GtkWidget *terminal);
 static void response(GtkWidget *dialog, gint response_id, GtkWidget *terminal);

 GAction * pw3270_action_save_session_as_new(void) {

	V3270SimpleAction * action = v3270_dialog_action_new(factory);

	action->name = "save.session.as";
	action->label = _("Save As");
	action->icon_name = "document-save-as";
	action->tooltip = _("Save session preferences");

	return G_ACTION(action);

 }

 GtkWidget * factory(V3270SimpleAction *action, GtkWidget *terminal) {

	GtkWidget *	dialog =
		gtk_file_chooser_dialog_new(
				action->tooltip,
				GTK_WINDOW(gtk_widget_get_toplevel(terminal)),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				_("Save"), GTK_RESPONSE_OK,
				_("Cancel"),GTK_RESPONSE_CANCEL,
				NULL
		);

	gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);
	gtk_file_chooser_set_pw3270_filters(GTK_FILE_CHOOSER(dialog));

	if(terminal) {
		g_autofree gchar * filename = v3270_key_file_build_filename(terminal);
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),filename);
	}

	g_signal_connect(dialog,"response",G_CALLBACK(response),terminal);

	gtk_widget_show_all(dialog);
	return dialog;
 }

 void response(GtkWidget *dialog, gint response_id, GtkWidget *terminal) {

	debug("%s(%d)",__FUNCTION__,response_id);

	g_autofree gchar * filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

	gtk_widget_destroy(dialog);

	if(response_id == GTK_RESPONSE_OK) {
		GError * error = NULL;
		v3270_key_file_open(terminal,filename,&error);

		if(error) {

			GtkWidget * dialog = gtk_message_dialog_new_with_markup(
											GTK_WINDOW(gtk_widget_get_toplevel(terminal)),
											GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
											GTK_MESSAGE_ERROR,
											GTK_BUTTONS_CANCEL,
											_("Can't open \"%s\""),filename
										);

			gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),"%s",error->message);

			gtk_window_set_title(GTK_WINDOW(dialog),_("Can't load session file"));

			gtk_widget_show_all(dialog);

			g_signal_connect(dialog,"close",G_CALLBACK(gtk_widget_destroy),NULL);
			g_signal_connect(dialog,"response",G_CALLBACK(gtk_widget_destroy),NULL);

			g_error_free(error);
		}

	}

 }

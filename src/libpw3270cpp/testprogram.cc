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
 * Este programa está nomeado como testprogram.cc e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */

 #include <pw3270cpp.h>
 #include <unistd.h>
 #include <iostream>

 using namespace std;
 using namespace PW3270_NAMESPACE;

/*--[ Implement ]------------------------------------------------------------------------------------*/

 int main(int numpar, char *param[])
 {

	try
 	{
		string	 s;
	//	session	*session = session::start("service://?");
		session	*session = session::start("");
	// 	session	*session = session::start("pw3270:A");

		string name = session->get_session_name();

		cout << "pw3270 version:  " << session->get_version() << endl;
		cout << "pw3270 revision: " << session->get_revision() << endl;
		cout << "pw3270 session:  " << name << endl << endl;

		session->set_timeout(60);
		session->set_autoclose(60);

		if(session->is_connected())
			cout << "\tConnected to host" << endl;
		else
			cout << "\tDisconnected" << endl;

		cout << "\tSession state:   " << session->get_cstate() << endl;

		// s = session->get_display_charset();
		// cout << "\tDisplay charset: " << s.c_str() << endl;

		// s = session->get_host_charset();
		// cout << "\tHost charset:    " << s.c_str() << endl;

		cout << "Connect: " << session->connect("tn3270://fandezhi.efglobe.com:23",60) << endl << endl;

		cout << "\tWaitForReady:	" << session->wait_for_ready(10) << endl;

		cout << "\tIsConnected:		" << session->is_connected() << endl;
		cout << "\tIsReady:			" << session->is_ready() << endl;
		cout << "\tString(1,2,26)	" << session->get_string_at(1,2,26) << endl;

		cout << "Conteúdo:" << endl << session->get_contents() << endl;

		delete session;

		/*
		session = session::start(name.c_str());
		cout << "Restored session:  " << name << endl << endl;
		cout << "\tIsConnected:		" << session->is_connected() << endl;
		cout << "\tIsReady:			" << session->is_ready() << endl;

		session->disconnect();
		session->close();
		delete session;
		*/

 	}
 	catch(std::exception &e) {

		cerr << e.what() << endl;
		return -1;
 	}

	// Waits
	sleep(2);

	// Create another session
	/*
	{
		session	*session = session::start("pw3270:a");

		session->disconnect();
		delete session;

	}
	*/


 	return 0;
 }



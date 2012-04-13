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
 * programa;  se  não, escreva para a Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA, 02111-1307, USA
 *
 * Este programa está nomeado como selection.c e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */

 #include "globals.h"
// #include "ctlr.h"
 #include "appres.h"
 #include <lib3270.h>
 #include <lib3270/session.h>
 #include <lib3270/selection.h>

 #define SELECTION_LEFT		0x01
 #define SELECTION_TOP		0x02
 #define SELECTION_RIGHT	0x04
 #define SELECTION_BOTTOM	0x08
 #define SELECTION_ACTIVE	0x10

 static void select_region(H3270 *h, int start, int end);

 /*--[ Implement ]------------------------------------------------------------------------------------*/

static void get_selected_addr(H3270 *session, int *start, int *end)
{
	if(session->select.start > session->select.end)
	{
		*end   = session->select.start;
		*start = session->select.end;
	}
	else
	{
		*start = session->select.start;
		*end   = session->select.end;
	}
}

static void update_selected_rectangle(H3270 *session)
{
	struct
	{
		int row;
		int col;
	} p[2];


	int begin, end, row, col, baddr;

	get_selected_addr(session,&begin,&end);

	// Get start & end posision
	p[0].row = (begin/session->cols);
	p[0].col = (begin%session->cols);
	p[1].row = (end/session->cols);
	p[1].col = (end%session->cols);

	if(p[0].row > p[1].row)
	{
		int swp = p[0].row;
		p[0].row = p[1].row;
		p[1].row = swp;
	}

	if(p[0].col > p[1].col)
	{
		int swp = p[0].col;
		p[0].col = p[1].col;
		p[1].col = swp;
	}

	// First remove unselected areas
	baddr = 0;
	for(row=0;row < session->rows;row++)
	{
		for(col = 0; col < session->cols;col++)
		{
			if(!(row >= p[0].row && row <= p[1].row && col >= p[0].col && col <= p[1].col) && (session->text[baddr].attr & LIB3270_ATTR_SELECTED))
			{
				session->text[baddr].attr &= ~LIB3270_ATTR_SELECTED;
				session->update(session,baddr,session->text[baddr].chr,session->text[baddr].attr,baddr == session->cursor_addr);
			}
			baddr++;
		}
	}

	// Then, draw selected ones
	baddr = 0;
	for(row=0;row < session->rows;row++)
	{
		for(col = 0; col < session->cols;col++)
		{
			if((row >= p[0].row && row <= p[1].row && col >= p[0].col && col <= p[1].col) && !(session->text[baddr].attr & LIB3270_ATTR_SELECTED))
			{
				session->text[baddr].attr |= LIB3270_ATTR_SELECTED;
				session->update(session,baddr,session->text[baddr].chr,session->text[baddr].attr,baddr == session->cursor_addr);
			}
			baddr++;
		}
	}

}

static void update_selected_region(H3270 *session)
{
	int baddr,begin,end;
	int len = session->rows*session->cols;

	get_selected_addr(session,&begin,&end);

	// First remove unselected areas
	for(baddr = 0; baddr < begin; baddr++)
	{
		if(session->text[baddr].attr & LIB3270_ATTR_SELECTED)
		{
			session->text[baddr].attr &= ~LIB3270_ATTR_SELECTED;
			session->update(session,baddr,session->text[baddr].chr,session->text[baddr].attr,baddr == session->cursor_addr);
		}
	}

	for(baddr = end+1; baddr < len; baddr++)
	{
		if(session->text[baddr].attr & LIB3270_ATTR_SELECTED)
		{
			session->text[baddr].attr &= ~LIB3270_ATTR_SELECTED;
			session->update(session,baddr,session->text[baddr].chr,session->text[baddr].attr,baddr == session->cursor_addr);
		}
	}

	// Then draw the selected ones
	for(baddr = begin; baddr <= end; baddr++)
	{
		if(!(session->text[baddr].attr & LIB3270_ATTR_SELECTED))
		{
			session->text[baddr].attr |= LIB3270_ATTR_SELECTED;
			session->update(session,baddr,session->text[baddr].chr,session->text[baddr].attr,baddr == session->cursor_addr);
		}
	}

}

void toggle_rectselect(H3270 *session, struct lib3270_toggle *t, LIB3270_TOGGLE_TYPE tt)
{
	if(!session->selected)
		return;

	if(t->value)
		update_selected_rectangle(session);
	else
		update_selected_region(session);
}

LIB3270_ACTION(unselect)
{
	int a;

	CHECK_SESSION_HANDLE(hSession);

	if(hSession->selected)
	{
		hSession->selected = 0;

		for(a = 0; a < hSession->rows*hSession->cols; a++)
		{
			if(hSession->text[a].attr & LIB3270_ATTR_SELECTED)
			{
				hSession->text[a].attr &= ~LIB3270_ATTR_SELECTED;
				if(hSession->update)
					hSession->update(hSession,a,hSession->text[a].chr,hSession->text[a].attr,a == hSession->cursor_addr);
			}
		}

		hSession->set_selection(hSession,0);
		hSession->update_selection(hSession,-1,-1);
	}

	return 0;
}

LIB3270_EXPORT void lib3270_select_to(H3270 *session, int baddr)
{
	int start, end;

	CHECK_SESSION_HANDLE(session);

	if(!lib3270_connected(session))
		return;

	start = session->selected ? session->select.start : session->cursor_addr;

	cursor_move(session,end = baddr);

	select_region(session,start,end);

}

LIB3270_EXPORT void lib3270_select_region(H3270 *h, int start, int end)
{
	int maxlen;

	CHECK_SESSION_HANDLE(h);

	if(!lib3270_connected(h))
		return;

	maxlen = (h->rows * h->cols);

	// Check bounds
	if(start < 0 || start > maxlen || end < 0 || end > maxlen || start > end)
		return;

	select_region(h,start,end);
	cursor_move(h,h->select.end);

}

static void select_region(H3270 *h, int start, int end)
{

	// Do we really need to change selection?
	if(start == h->select.start && end == h->select.end && h->selected)
		return;

	h->select.start		= start;
	h->select.end 		= end;

	if(lib3270_get_toggle(h,LIB3270_TOGGLE_RECTANGLE_SELECT))
		update_selected_rectangle(h);
	else
		update_selected_region(h);

	if(!h->selected)
	{
		h->selected = 1;
		h->set_selection(h,1);
	}

	h->update_selection(h,start,end);

}

LIB3270_EXPORT unsigned char lib3270_get_selection_flags(H3270 *hSession, int baddr)
{
	int row,col;
	unsigned char rc = 0;

	CHECK_SESSION_HANDLE(hSession);

	if(!(lib3270_connected(hSession) && (hSession->text[baddr].attr & LIB3270_ATTR_SELECTED)))
		return rc;

	row = baddr / hSession->cols;
	col = baddr % hSession->cols;
	rc |= SELECTION_ACTIVE;

	if( (col == 0) || !(hSession->text[baddr-1].attr & LIB3270_ATTR_SELECTED) )
		rc |= SELECTION_LEFT;

	if( (row == 0) || !(hSession->text[baddr-hSession->cols].attr & LIB3270_ATTR_SELECTED) )
		rc |= SELECTION_TOP;

	if( (col == hSession->cols) || !(hSession->text[baddr+1].attr & LIB3270_ATTR_SELECTED) )
		rc |= SELECTION_RIGHT;

	if( (row == hSession->rows) || !(hSession->text[baddr+hSession->cols].attr & LIB3270_ATTR_SELECTED) )
		rc |= SELECTION_BOTTOM;

	return rc;
}

LIB3270_EXPORT void lib3270_select_word(H3270 *session, int baddr)
{
	int pos, len, start, end;

	CHECK_SESSION_HANDLE(session);

	if(!lib3270_connected(session) || isspace(session->text[baddr].chr))
	{
		lib3270_ring_bell(session);
		return;
	}

	start = session->select.start;
	for(pos = baddr; pos > 0 && !isspace(session->text[pos].chr);pos--);
	start = pos > 0 ? pos+1 : 0;

	len = session->rows * session->cols;
	for(pos = baddr; pos < len && !isspace(session->text[pos].chr);pos++);
	end = pos < len ? pos-1 : len;

	select_region(session,start,end);
}

LIB3270_EXPORT int lib3270_select_field_at(H3270 *session, int baddr)
{
	int start, end,len;

	CHECK_SESSION_HANDLE(session);

	if(!lib3270_connected(session))
	{
		lib3270_ring_bell(session);
		return -1;
	}

	start = lib3270_field_addr(session,baddr);

	if(start < 0)
	{
		lib3270_ring_bell(session);
		return -1;
	}

	start++;

	len = (session->rows * session->cols)-1;
	end	= start + lib3270_field_length(session,start);
	if(end > len)
		end = len;

	select_region(session,start,end);

	return 0;
}

LIB3270_ACTION( selectfield )
{
	CHECK_SESSION_HANDLE(hSession);
	lib3270_select_field_at(hSession,hSession->cursor_addr);
	return 0;
}

LIB3270_ACTION( selectall )
{
	int len, baddr;

	CHECK_SESSION_HANDLE(hSession);

	select_region(hSession,0,hSession->rows*hSession->cols);
/*
	len = hSession->rows*hSession->cols;

	for(baddr = 0; baddr < len; baddr++)
	{
		if(!(hSession->text[baddr].attr & LIB3270_ATTR_SELECTED))
		{
			hSession->text[baddr].attr |= LIB3270_ATTR_SELECTED;
			hSession->update(hSession,baddr,hSession->text[baddr].chr,hSession->text[baddr].attr,baddr == hSession->cursor_addr);
		}
	}

	set_selected(hSession);
*/
	return 0;
}

LIB3270_ACTION( reselect )
{
	int start, end;

	CHECK_SESSION_HANDLE(hSession);

	if(!lib3270_connected(hSession) || hSession->select.start == hSession->select.end || hSession->selected)
		return 0;

	select_region(hSession, hSession->select.start,hSession->select.end);

	return 0;
}

static char * get_text(H3270 *hSession,unsigned char all)
{
	int	row, col, baddr;
	char *ret;
	size_t buflen = (hSession->rows * (hSession->cols+1))+1;
	size_t sz = 0;

	if(!lib3270_connected(hSession))
		return NULL;

	ret = malloc(buflen);

	baddr = 0;
	for(row=0;row < hSession->rows;row++)
	{
		int cr = 0;

		for(col = 0; col < hSession->cols;col++)
		{
			if(all || hSession->text[baddr].attr & LIB3270_ATTR_SELECTED)
			{
				cr++;
				ret[sz++] = hSession->text[baddr].chr;
			}
			baddr++;
		}

		if(cr)
			ret[sz++] = '\n';
	}

	if(!sz)
	{
		free(ret);
		return NULL;
	}

	ret[sz++] = 0;

	if(sz != buflen)
		ret = realloc(ret,sz);

	return ret;
}

LIB3270_EXPORT char * lib3270_get_region(H3270 *h, int start_pos, int end_pos, unsigned char all)
{
	char *	text;
	int 	maxlen;
	int		sz = 0;
	int		baddr;

	CHECK_SESSION_HANDLE(h);

	if(!lib3270_connected(h))
		return NULL;

	maxlen = h->rows * (h->cols+1);

	if(start_pos < 0 || start_pos > maxlen || end_pos < 0 || end_pos > maxlen || end_pos < start_pos)
		return NULL;

	text = malloc(maxlen);

	for(baddr=start_pos;baddr<end_pos;baddr++)
	{
		if(all || h->text[baddr].attr & LIB3270_ATTR_SELECTED)
			text[sz++] = (h->text[baddr].attr & LIB3270_ATTR_CG) ? ' ' : h->text[baddr].chr;

		if((baddr%h->cols) == 0 && sz > 0)
			text[sz++] = '\n';
	}
	text[sz++] = 0;

	return realloc(text,sz);
}




LIB3270_EXPORT char * lib3270_get_text(H3270 *h, int offset, int len)
{
	char * buffer;
	int    maxlen;
	char * ptr;

	CHECK_SESSION_HANDLE(h);

	if(!lib3270_connected(h))
		return NULL;

	maxlen = h->rows * (h->cols+1);

	if(len < 0)
		len = (maxlen - offset);
	else if(len > maxlen)
		len = maxlen;

	buffer	= malloc(len+1);
	ptr		= buffer;

//	trace("len=%d buffer=%p",len,buffer);

	while(len > 0)
	{
		if(h->text[offset].attr & LIB3270_ATTR_CG)
			*ptr = ' ';
		else if(h->text[offset].chr)
			*ptr = h->text[offset].chr;
		else
			*ptr = ' ';

		ptr++;
		offset++;
		len--;

		if((offset%h->cols) == 0 && len > 0)
		{
			*(ptr++) = '\n';
			len--;
		}
	}
//	trace("len=%d buffer=%p pos=%d",len,buffer,ptr-buffer);

	*ptr = 0;

	return buffer;
}

LIB3270_EXPORT char * lib3270_get_selected(H3270 *hSession)
{
	if(!hSession->selected || hSession->select.start == hSession->select.end)
		return NULL;

	if(!lib3270_connected(hSession))
		return NULL;


	return get_text(hSession,0);
}

LIB3270_EXPORT int lib3270_get_selection_bounds(H3270 *hSession, int *start, int *end)
{
	int first, last;

	CHECK_SESSION_HANDLE(hSession);

	if(!hSession->selected || hSession->select.start == hSession->select.end)
		return 0;

	if(hSession->select.end > hSession->select.start)
	{
		first = hSession->select.start;
		last  = hSession->select.end;
	}
	else
	{
		first = hSession->select.end;
		last  = hSession->select.start;
	}

	if(start)
		*start = first;

	if(end)
		*end = last;

	return 1;
}

LIB3270_EXPORT int lib3270_move_selected_area(H3270 *hSession, int from, int to)
{
	int pos[2];
	int rows, cols, f, step, start, end;

	if(!lib3270_get_selection_bounds(hSession,&pos[0],&pos[1]))
		return from;

	rows = (to / hSession->cols) - (from / hSession->cols);
	cols = (to % hSession->cols) - (from % hSession->cols);

	for(f=0;f<2;f++)
	{
		int row  = (pos[f] / hSession->cols) + rows;
		int col  = (pos[f] % hSession->cols) + cols;

		if(row < 0)
			rows = - (pos[f] / hSession->cols);

		if(col < 0)
			cols = - (pos[f] % hSession->cols);

		if(row >= (hSession->rows))
			rows = hSession->rows - ((pos[f] / hSession->cols)+1);

		if(col >= hSession->cols)
			cols = hSession->cols - ((pos[f] % hSession->cols)+1);
	}

	step 	= (rows * hSession->cols) + cols;

	select_region(hSession,hSession->select.start + step,hSession->select.end + step);
	cursor_move(hSession,hSession->select.end);

	return from+step;
}

LIB3270_EXPORT int lib3270_drag_selection(H3270 *h, unsigned char flag, int origin, int baddr)
{
	int first, last, row, col;

	if(!lib3270_get_selection_bounds(h,&first,&last))
		return origin;

	flag &= 0x1f;

	trace("%s: flag=%04x",__FUNCTION__,flag);

	if(!flag)
		return origin;
	else if(flag == SELECTION_ACTIVE)
		return lib3270_move_selected_area(h,origin,baddr);

	row = baddr/h->cols;
	col = baddr%h->cols;

	if(flag & SELECTION_LEFT)		// Update left margin
		origin = first = ((first/h->cols)*h->cols) + col;

	if(flag & SELECTION_TOP)		// Update top margin
		origin = first = (row*h->cols) + (first%h->cols);

	if(flag & SELECTION_RIGHT) 		// Update right margin
		origin = last = ((last/h->cols)*h->cols) + col;

	if(flag & SELECTION_BOTTOM)		// Update bottom margin
		origin = last = (row*h->cols) + (last%h->cols);

	if(first < last)
		select_region(h,first,last);
	else
		select_region(h,last,first);

	cursor_move(h,h->select.end);

	return origin;
}


LIB3270_EXPORT int lib3270_move_selection(H3270 *hSession, LIB3270_DIRECTION dir)
{
	int start, end;

	if(!hSession->selected || hSession->select.start == hSession->select.end)
		return ENOENT;

	start = hSession->select.start;
	end   = hSession->select.end;

	switch(dir)
	{
	case LIB3270_DIR_UP:
		if(start <= hSession->cols)
			return EINVAL;
		start -= hSession->cols;
		end   -= hSession->cols;
		break;

	case LIB3270_DIR_DOWN:
		if(end >= (hSession->cols * (hSession->rows-1)))
			return EINVAL;
		start += hSession->cols;
		end   += hSession->cols;
		break;

	case LIB3270_DIR_LEFT:
		if( (start % hSession->cols) < 1)
			return EINVAL;
		start--;
		end--;
		break;

	case LIB3270_DIR_RIGHT:
		if( (end % hSession->cols) >= (hSession->cols-1))
			return EINVAL;
		start++;
		end++;
		break;

	default:
		return -1;
	}

	select_region(hSession,start,end);
	cursor_move(hSession,hSession->select.end);

	return 0;
}

LIB3270_EXPORT int lib3270_move_cursor(H3270 *hSession, LIB3270_DIRECTION dir, unsigned char sel)
{
	int cursor_addr = hSession->cursor_addr;

	if(!lib3270_connected(hSession))
		return -1;

	switch(dir)
	{
	case LIB3270_DIR_UP:
		if(cursor_addr <= hSession->cols)
			return EINVAL;
		cursor_addr -= hSession->cols;
		break;

	case LIB3270_DIR_DOWN:
		if(cursor_addr >= (hSession->cols * (hSession->rows-1)))
			return EINVAL;
		cursor_addr += hSession->cols;
		break;

	case LIB3270_DIR_LEFT:
		if( (cursor_addr % hSession->cols) < 1)
			return EINVAL;
		cursor_addr--;
		break;

	case LIB3270_DIR_RIGHT:
		if( (cursor_addr % hSession->cols) >= (hSession->cols-1))
			return EINVAL;
		cursor_addr++;
		break;

	default:
		return -1;
	}

	if(sel)
		lib3270_select_to(hSession,cursor_addr);
	else
		cursor_move(hSession,cursor_addr);
}


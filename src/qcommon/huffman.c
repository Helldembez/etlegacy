/*
 * Wolfenstein: Enemy Territory GPL Source Code
 * Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
 *
 * ET: Legacy
 * Copyright (C) 2012-2023 ET:Legacy team <mail@etlegacy.com>
 *
 * This file is part of ET: Legacy - http://www.etlegacy.com
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ET: Legacy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ET: Legacy. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, Wolfenstein: Enemy Territory GPL Source Code is also
 * subject to certain additional terms. You should have received a copy
 * of these additional terms immediately following the terms and conditions
 * of the GNU General Public License which accompanied the source code.
 * If not, please request a copy in writing from id Software at the address below.
 *
 * id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
 */
/**
 * @file huffman.c
 * @brief This is based on the Adaptive Huffman algorithm described in Sayood's
 *        Data Compression book.  The ranks are not actually stored, but implicitly
 *        defined by the location of a node within a doubly-linked list
 */

#include "q_shared.h"
#include "qcommon.h"

static int bloc = 0;

/**
 * @brief Clears data along the way so we dont have to memset() it ahead of time
 * @param[in] bit
 * @param[out] fout
 * @param[out] offset
 */
void Huff_putBit(int bit, byte *fout, int *offset)
{
	int x, y;

	bloc = *offset;
	x    = bloc >> 3;
	y    = bloc & 7;
	if (!y)
	{
		fout[x] = 0;
	}
	fout[x] |= bit << y;
	bloc++;
	*offset = bloc;
}

/**
 * @brief Huff_getBit
 * @param[in] fin
 * @param[in,out] offset
 * @return
 */
int Huff_getBit(byte *fin, int *offset)
{
	int t;

	bloc = *offset;
	t    = fin[bloc >> 3] >> (bloc & 7) & 0x1;
	bloc++;
	*offset = bloc;
	return t;
}

/**
 * @brief Clears data along the way so we dont have to memset() it ahead of time
 *
 * @param[in] bit
 * @param[out] fout
 */
static void add_bit(const char bit, byte *fout)
{
	int x, y;

	y = bloc >> 3;
	x = bloc++ & 7;
	if (!x)
	{
		fout[y] = 0;
	}
	fout[y] |= bit << x;
}

/**
 * @brief get_bit
 * @param[in] fin
 * @return
 */
static int get_bit(byte *fin)
{
	int t;

	t = fin[bloc >> 3] >> (bloc & 7) & 0x1;
	bloc++;
	return t;
}

/**
 * @brief get_ppnode
 * @param[in,out] huff
 * @return
 */
static node_t **get_ppnode(huff_t *huff)
{
	if (!huff->freelist)
	{
		return &(huff->nodePtrs[huff->blocPtrs++]);
	}
	else
	{
		node_t **tppnode = huff->freelist;

		huff->freelist = (node_t **)*tppnode;
		return tppnode;
	}
}

/**
 * @brief free_ppnode
 * @param[in,out] huff
 * @param[in,out] ppnode
 */
static void free_ppnode(huff_t *huff, node_t **ppnode)
{
	*ppnode        = (node_t *)huff->freelist;
	huff->freelist = ppnode;
}

/**
 * @brief Swap the location of these two nodes in the tree
 * @param[out] huff
 * @param[in,out] node1
 * @param[in,out] node2
 */
static void swap(huff_t *huff, node_t *node1, node_t *node2)
{
	node_t *par1 = node1->parent;
	node_t *par2 = node2->parent;

	if (par1)
	{
		if (par1->left == node1)
		{
			par1->left = node2;
		}
		else
		{
			par1->right = node2;
		}
	}
	else
	{
		huff->tree = node2;
	}

	if (par2)
	{
		if (par2->left == node2)
		{
			par2->left = node1;
		}
		else
		{
			par2->right = node1;
		}
	}
	else
	{
		huff->tree = node1;
	}

	node1->parent = par2;
	node2->parent = par1;
}

/**
 * @brief Swap these two nodes in the linked list (update ranks)
 * @param[in,out] node1
 * @param[in,out] node2
 */
static void swaplist(node_t *node1, node_t *node2)
{
	node_t *par1 = node1->next;

	node1->next = node2->next;
	node2->next = par1;

	par1        = node1->prev;
	node1->prev = node2->prev;
	node2->prev = par1;

	if (node1->next == node1)
	{
		node1->next = node2;
	}
	if (node2->next == node2)
	{
		node2->next = node1;
	}
	if (node1->next)
	{
		node1->next->prev = node1;
	}
	if (node2->next)
	{
		node2->next->prev = node2;
	}
	if (node1->prev)
	{
		node1->prev->next = node1;
	}
	if (node2->prev)
	{
		node2->prev->next = node2;
	}
}

/**
 * @brief Do the increments
 * @param[in] huff
 * @param[in,out] node
 */
static void increment(huff_t *huff, node_t *node)
{
	if (!node)
	{
		return;
	}

	if (node->next != NULL && node->next->weight == node->weight)
	{
		node_t *lnode = *node->head;

		if (lnode != node->parent)
		{
			swap(huff, lnode, node);
		}
		swaplist(lnode, node);
	}
	if (node->prev && node->prev->weight == node->weight)
	{
		*node->head = node->prev;
	}
	else
	{
		*node->head = NULL;
		free_ppnode(huff, node->head);
	}
	node->weight++;
	if (node->next && node->next->weight == node->weight)
	{
		node->head = node->next->head;
	}
	else
	{
		node->head  = get_ppnode(huff);
		*node->head = node;
	}
	if (node->parent)
	{
		increment(huff, node->parent);
		if (node->prev == node->parent)
		{
			swaplist(node, node->parent);
			if (*node->head == node)
			{
				*node->head = node->parent;
			}
		}
	}
}

/**
 * @brief Huff_addRef
 * @param[in,out] huff
 * @param[in] ch
 */
void Huff_addRef(huff_t *huff, byte ch)
{
	if (huff->loc[ch] == NULL)     // if this is the first transmission of this node
	{
		node_t *tnode  = &(huff->nodeList[huff->blocNode++]);
		node_t *tnode2 = &(huff->nodeList[huff->blocNode++]);

		tnode2->symbol = INTERNAL_NODE;
		tnode2->weight = 1;
		tnode2->next   = huff->lhead->next;
		if (huff->lhead->next)
		{
			huff->lhead->next->prev = tnode2;
			if (huff->lhead->next->weight == 1)
			{
				tnode2->head = huff->lhead->next->head;
			}
			else
			{
				tnode2->head  = get_ppnode(huff);
				*tnode2->head = tnode2;
			}
		}
		else
		{
			tnode2->head  = get_ppnode(huff);
			*tnode2->head = tnode2;
		}
		huff->lhead->next = tnode2;
		tnode2->prev      = huff->lhead;

		tnode->symbol = ch;
		tnode->weight = 1;
		tnode->next   = huff->lhead->next;
		if (huff->lhead->next)
		{
			huff->lhead->next->prev = tnode;
			if (huff->lhead->next->weight == 1)
			{
				tnode->head = huff->lhead->next->head;
			}
			else
			{
				// this should never happen
				tnode->head  = get_ppnode(huff);
				*tnode->head = tnode2;
			}
		}
		else
		{
			// this should never happen
			tnode->head  = get_ppnode(huff);
			*tnode->head = tnode;
		}
		huff->lhead->next = tnode;
		tnode->prev       = huff->lhead;
		tnode->left       = tnode->right = NULL;

		if (huff->lhead->parent)
		{
			if (huff->lhead->parent->left == huff->lhead)     // lhead is guaranteed to by the NYT
			{
				huff->lhead->parent->left = tnode2;
			}
			else
			{
				huff->lhead->parent->right = tnode2;
			}
		}
		else
		{
			huff->tree = tnode2;
		}

		tnode2->right = tnode;
		tnode2->left  = huff->lhead;

		tnode2->parent      = huff->lhead->parent;
		huff->lhead->parent = tnode->parent = tnode2;

		huff->loc[ch] = tnode;

		increment(huff, tnode2->parent);
	}
	else
	{
		increment(huff, huff->loc[ch]);
	}
}

/**
 * @brief Get a symbol
 * @param[in] node
 * @param[out] ch
 * @param[in] fin
 * @return
 */
int Huff_Receive(node_t *node, int *ch, byte *fin)
{
	while (node && node->symbol == INTERNAL_NODE)
	{
		if (get_bit(fin))
		{
			node = node->right;
		}
		else
		{
			node = node->left;
		}
	}
	if (!node)
	{
		return 0;
		//Com_Error(ERR_DROP, "Illegal tree!");
	}
	return (*ch = node->symbol);
}

/**
 * @brief Get a symbol
 * @param[in] node
 * @param[out] ch
 * @param[in] fin
 * @param[out] offset
 * @param[in] maxoffset
 */
void Huff_offsetReceive(node_t *node, int *ch, byte *fin, int *offset, int maxoffset)
{
	bloc = *offset;
	while (node && node->symbol == INTERNAL_NODE)
	{
		if (bloc >= maxoffset)
		{
			*ch = 0;
			*offset = maxoffset + 1;
			return;
		}
		if (get_bit(fin))
		{
			node = node->right;
		}
		else
		{
			node = node->left;
		}
	}
	if (!node)
	{
		*ch = 0;
		return;
		//Com_Error(ERR_DROP, "Illegal tree!");
	}
	*ch     = node->symbol;
	*offset = bloc;
}

/**
 * @brief Send the prefix code for this node
 * @param[in] node
 * @param[in] child
 * @param[in] fout
 * @param[in] maxoffset
 */
static void send(node_t *node, node_t *child, byte *fout, int maxoffset)
{
	if (node->parent)
	{
		send(node->parent, node, fout, maxoffset);
	}
	if (child)
	{
		if (bloc >= maxoffset)
		{
			bloc = maxoffset + 1;
			return;
		}
		if (node->right == child)
		{
			add_bit(1, fout);
		}
		else
		{
			add_bit(0, fout);
		}
	}
}

/**
 * @brief Huff_transmit
 * @param[in] huff
 * @param[in] ch
 * @param[out] fout
 * @param[in] maxoffset
 */
void Huff_transmit(huff_t *huff, int ch, byte *fout, int maxoffset)
{
	if (huff->loc[ch] == NULL)
	{
		int i;

		// node_t hasn't been transmitted, send a NYT, then the symbol
		Huff_transmit(huff, NYT, fout, maxoffset);
		for (i = 7; i >= 0; i--)
		{
			add_bit((char)((ch >> i) & 0x1), fout);
		}
	}
	else
	{
		send(huff->loc[ch], NULL, fout, maxoffset);
	}
}

/**
 * @brief Huff_offsetTransmit
 * @param[in] huff
 * @param[in] ch
 * @param[out] fout
 * @param[in,out] offset
 * @param[in] maxoffset
 */
void Huff_offsetTransmit(huff_t *huff, int ch, byte *fout, int *offset, int maxoffset)
{
	bloc = *offset;
	send(huff->loc[ch], NULL, fout, maxoffset);
	*offset = bloc;
}

/**
 * @brief Huff_Decompress
 * @param[in,out] mbuf
 * @param[in] offset
 */
void Huff_Decompress(msg_t *mbuf, int offset)
{
	int    ch, cch, i, j, size;
	byte   seq[65536];
	byte   *buffer;
	huff_t huff;

	size   = mbuf->cursize - offset;
	buffer = mbuf->data + offset;

	if (size <= 0)
	{
		return;
	}

	Com_Memset(&huff, 0, sizeof(huff_t));
	// Initialize the tree & list with the NYT node
	huff.tree         = huff.lhead = huff.ltail = huff.loc[NYT] = &(huff.nodeList[huff.blocNode++]);
	huff.tree->symbol = NYT;
	huff.tree->weight = 0;
	huff.lhead->next  = huff.lhead->prev = NULL;
	huff.tree->parent = huff.tree->left = huff.tree->right = NULL;

	cch = buffer[0] * 256 + buffer[1];
	// don't overflow with bad messages
	if (cch > mbuf->maxsize - offset)
	{
		cch = mbuf->maxsize - offset;
	}
	bloc = 16;

	for (j = 0; j < cch; j++)
	{
		ch = 0;
		// don't overflow reading from the messages
		// FIXME: would it be better to have a overflow check in get_bit ?
		if ((bloc >> 3) > size)
		{
			seq[j] = 0;
			break;
		}
		Huff_Receive(huff.tree, &ch, buffer);           // Get a character
		if (ch == NYT)                                  // We got a NYT, get the symbol associated with it
		{
			ch = 0;
			for (i = 0; i < 8; i++)
			{
				ch = (ch << 1) + get_bit(buffer);
			}
		}

		seq[j] = ch;                                    // Write symbol

		Huff_addRef(&huff, (byte)ch);                   // Increment node
	}
	mbuf->cursize = cch + offset;
	Com_Memcpy(mbuf->data + offset, seq, cch);
}

extern int oldsize;

/**
 * @brief Huff_Compress
 * @param[in,out] mbuf
 * @param[in] offset
 */
void Huff_Compress(msg_t *mbuf, int offset)
{
	int    i, ch, size;
	byte   seq[65536];
	byte   *buffer;
	huff_t huff;

	size   = mbuf->cursize - offset;
	buffer = mbuf->data + offset;

	if (size <= 0)
	{
		return;
	}

	Com_Memset(&huff, 0, sizeof(huff_t));
	// Add the NYT (not yet transmitted) node into the tree/list
	huff.tree         = huff.lhead = huff.loc[NYT] = &(huff.nodeList[huff.blocNode++]);
	huff.tree->symbol = NYT;
	huff.tree->weight = 0;
	huff.lhead->next  = huff.lhead->prev = NULL;
	huff.tree->parent = huff.tree->left = huff.tree->right = NULL;
	huff.loc[NYT]     = huff.tree;

	seq[0] = (size >> 8);
	seq[1] = size & 0xff;

	bloc = 16;

	for (i = 0; i < size; i++)
	{
		ch = buffer[i];
		Huff_transmit(&huff, ch, seq, size<<3);  // Transmit symbol
		Huff_addRef(&huff, (byte)ch);   // Do update
	}

	bloc += 8; // next byte

	mbuf->cursize = (bloc >> 3) + offset;
	Com_Memcpy(mbuf->data + offset, seq, (bloc >> 3));
}

/**
 * @brief Huff_Init
 * @param[in,out] huff
 */
void Huff_Init(huffman_t *huff)
{
	Com_Memset(&huff->compressor, 0, sizeof(huff_t));
	Com_Memset(&huff->decompressor, 0, sizeof(huff_t));

	// Initialize the tree & list with the NYT node
	huff->decompressor.tree         = huff->decompressor.lhead = huff->decompressor.ltail = huff->decompressor.loc[NYT] = &(huff->decompressor.nodeList[huff->decompressor.blocNode++]);
	huff->decompressor.tree->symbol = NYT;
	huff->decompressor.tree->weight = 0;
	huff->decompressor.lhead->next  = huff->decompressor.lhead->prev = NULL;
	huff->decompressor.tree->parent = huff->decompressor.tree->left = huff->decompressor.tree->right = NULL;

	// Add the NYT (not yet transmitted) node into the tree/list
	huff->compressor.tree         = huff->compressor.lhead = huff->compressor.loc[NYT] = &(huff->compressor.nodeList[huff->compressor.blocNode++]);
	huff->compressor.tree->symbol = NYT;
	huff->compressor.tree->weight = 0;
	huff->compressor.lhead->next  = huff->compressor.lhead->prev = NULL;
	huff->compressor.tree->parent = huff->compressor.tree->left = huff->compressor.tree->right = NULL;
	huff->compressor.loc[NYT]     = huff->compressor.tree;
}

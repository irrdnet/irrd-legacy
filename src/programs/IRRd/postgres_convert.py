#!/usr/bin/env python

"""
Utility to move irrd db's into postgres
"""

import os
import sys
import pg

def usage():
	print
	print "USAGE: " + sys.argv[0] + " dbfile.db"
	print
	print "Script assumes localhost, current user, no password and"
	print "creates a db named 'dbfile'"
	exit(1)

def parse_generic_as(t, l):
	if t[:2] != 'AS':
		raise TypeError
	return int(t[2:])

def parse_generic_string(t, l):
	return str(t)

def parse_generic_int(t, l):
	return int(t)

def parse_timestamp(t, l):
	raise NotImplementedError

def parse_email(t, l):
	raise NotImplementedError

def parse_cidr(t, l):
	raise NotImplementedError

def parse_pgp(t, l):
	if t[:7] != 'PGPKEY-':
		raise TypeError
	return t[7:]

def parse_unused(t, l):
	print "ERR: Attempt to parse an unused field?"
	raise TypeError

types = {
	# Manually parsed from templates.c
	# aut-num object
	'aut-num':[['aut_num', 'integer', parse_generic_as],],
	'as-name':[['as_name', 'text', parse_generic_string],],
	'descr':[['descr', 'text', parse_generic_string],],
	'member-of':[['member_of', 'text', parse_generic_string],],
	'import':[['import', 'text', parse_generic_string],],
	'mp-import':[['mp_import', 'text', parse_generic_string],],
	'export':[['export', 'text', parse_generic_string],],
	'mp-export':[['mp_export', 'text', parse_generic_string],],
	'default':[['default', 'text', parse_generic_string],],
	'mp-default':[['mp_default', 'text', parse_generic_string],],
	'admin-c':[['admin_c', 'text', parse_generic_string],],
	'tech-c':[['tech_c', 'text', parse_generic_string],],
	'remarks':[['remarks', 'text', parse_generic_string],],
	'notify':[['notify', 'text', parse_email],],
	'mnt-by':[['mnt_by', 'text', parse_generic_string],],
	'changed':[['changed_contact', 'text', parse_generic_string], ['changed_timestamp', 'timestamp', parse_timestamp]],
	'source':[['source', 'text', parse_generic_string],],
	# as-set object
	'as-set':[['as_set', 'text', parse_generic_string],],
	'members':[['members', 'text', parse_generic_string],],
	'mbrs-by-ref':[['mbrs_by_ref', 'text', parse_generic_string],],
	# mntner
	'mntner':[['mntner', 'text', parse_generic_string],],
	'upd-to':[['update_to', 'text', parse_email],],
	'mnt-nfy':[['mnt_nfy', 'text', parse_email],],
	'auth':[['auth', 'text', parse_generic_string],], # XXX
	# route
	'route':[['route', 'cidr', parse_cidr],],
	'origin':[['origin', 'integer', parse_generic_as],],
	'holes':[['holes', 'cidr', parse_cidr],],
	'inject':[['inject', 'UNUSED', parse_unused],],
	'aggr-bndry':[['aggr_bndry', 'text', parse_generic_string],],
	'aggr-mtd':[['aggr_mtd', 'text', parse_generic_string],],
	'export-comps':[['export_comps', 'WTF', parse_wtf],], # XXX: cidr-ish?
	'components':[['components', 'WTF', parse_wtf],], # XXX: cidr-ish?
	'geoidx':[['geoidx', 'integer', parse_generic_int],],
	'cert-url':[['cert-url', 'text', parse_generic_string],],
	# route6
	'route6':[['route6', 'cidr', parse_cidr],],
	# route-set
	'route-set':[['route_set', 'text', parse_generic_string],],
	'mp-members':[['mp_members', 'cidr', parse_cidr],],
	# inet-rtr
	'inet-rtr':[['inet_rtr', 'text', parse_generic_string],], # inet OR url
	'local-as':[['local_as', 'integer', parse_generic_as],],
	'ifaddr':[['ifaddr', 'WTF', parse_wtf],], # 1.2.3.4 masklen 24 ??
	'interface':[['interface', 'WTF', parse_wtf],], # 1:2::4 masklen 64 ??
	'peer':[['peer', 'text', parse_generic_string],], # BGP# IP asno(AS##)
	'mp-peer':[['mp_pper', 'UNUSED', parse_unused],],
	'rs-in':[['rs_in', 'text', parse_generic_string],], # can be parsed further
	'rs-out':[['rs_out', 'text', parse_generic_string],], # can be parsed further
	# rtr-set
	'rtr-set':[['rtr_set', 'text', parse_generic_string],],
	# person
	'person':[['person', 'text', parse_generic_string],],
	'phone':[['phone', 'text', parse_generic_string],], # can be parse further
	'e-mail':[['e_mail', 'text', parse_email],],
	'nic-hdl':[['nic_hdl', 'text', parse_generic_string],],
	# role
	'role':[['role', 'text', parse_generic_string],],
	'trouble':[['trouble', 'text', parse_generic_string],],
	'address':[['address', 'text', parse_generic_string],], # XXX subdivide?
	'fax-no':[['fax_no', 'text', parse_generic_string],], # Can parse further
	# filter-set
	'filter-set':[['filter_set', 'text', parse_generic_string],],
	'filter':[['filter', 'text', parse_generic_string],],
	'mp-filter':[['mp_filter', 'UNUSED', parse_unused],],
	# peering-set
	'peering-set':[['peering_set', 'text', parse_generic_string],],
	'peering':[['peering', 'text', parse_generic_string],],
	'mp-peering':[['mp_peering', 'text', parse_generic_string],],
	# key-cert
	'key-cert':[['key_cert', 'char (8)', parse_pgp],],
	'method':[['method', 'text', parse_generic_string],], # XXX always 'PGP'?
	'owner':[['owner_name', 'text', parse_generic_string], ['owner_email', 'text', parse_email]],
	'fingerpr':[['fingerpr', 'text', parse_generic_string],],
	'certif':[['certif', 'UNUSED', parse_unused],],
	# dictionary
	'dictionary':[['dictionary', 'UNUSED', parse_unused],],
	# XXX - dict objects not finished
	# repositroy
	'repositroy':[['repositroy', 'UNUSED', parse_unused],],
	# XXX - repo objects not finished
	# inetnum
	'inetnum':[['inetnum_start', 'inet', parse_inet], ['inetnum_end', 'inet', parse_inet]],
	'netname':[['netname', 'text', parse_generic_string],],
	'country':[['country', 'text', parse_generic_string],], # 2-char abbr?
	'rev-srv':[['rev-srv', 'UNUSED', parse_unused],],
	'status':[['status', 'text', parse_generic_string],],
	# inet6num
	'inet6num':[['inet6num', 'UNUSED', parse_unused],],
	# XXX - inet6num objects not finished
	# as-block
	'as-block':[['as-block', 'UNUSED', parse_unused],],
	# XXX - as-block objects not finished
	# domain
	'domain':[['domain', 'UNUSED', parse_unused],],
	# XXX - domain objects not finished
	# limerick
	'limerick':[['limerick', 'UNUSED', parse_unused],],
	# XXX - limerick objects not finished
	# ipv6-site
	'ipv6-site':[['ipv6_site', 'UNUSED', parse_unused],],
	# XXX - ipv6-side objects not finished
	}

def template_parse_line(line):
	"""Convert db def line (from templates.c) into a tuple"""
	# Handle blank lines
	if line == '\n':
		return []

	# strip quotes, '\n' from file, and '\n' from readline
	line = line[1:-4]
	# handle last line w/ extra ,
	if line[-1] == '\\':
		line = line[:-1]
	ret,rest = line.split(':')
	ret = [ret]
	rest = rest.strip(" []")
	ret.append(rest.split(']')[0])
	rest = rest[len(ret[-1])+1:].strip(" [")
	ret.append(rest.split("]")[0])
	rest = rest[len(ret[-1])+1:].strip(" [")
	if len(rest):
		ret.append(rest)
	else:
		ret.append([])
	return ret

def template_parse(f):
	templates = open(f, 'r')
	if templates.readline() != '/* AUTOMATICALLY GENERATED - DO NOT EDIT - see ../irr_util/README */\n' or templates.readline() != '\n' or templates.readline() != '#include "scan.h"\n' or templates.readline() != '\n' or templates.readline() != 'char *obj_template[IRR_MAX_CLASS_KEYS] = {\n':
		print "Error in templates.c  -- header changed?"
		exit(2)
	
	table = None
	for line in templates:
		if line == '};\n':
			# end of array
			break
		line = template_parse(line)

		if line == []:
			table = None
			continue

		if table is None:
			table = line[0]
			tables[table] = {}

		# Handle some special cases
		if line[0] == 'address':
			tables[table]['addr_name'] = line[1:]
			tables[table]['street_no'] = line[1:]
			tables[table]['street_name'] = line[1:]
			tables[table]['city'] = line[1:]
			tables[table]['state'] = line[1:]
			tables[table]['zip'] = line[1:]
			tables[table]['country'] = line[1:]
			tables[table]['addr_xtra'] = line[1:]
		elif line[0] == 'import':
			tables[table]['import_from'] = line[1:]
			tables[table]['import_action'] = line[1:]
			tables[table]['import_accept'] = line[1:]
		elif line[0] == 'export':
			tables[table]['export_to'] = line[1:]
			tables[table]['export_announce'] = line[1:]
		elif line[0] == 'changed':
			pass
		else:
			tables[table][line[0]] = line[1:]

	return tables

def db_parse(fp):
	"""Convert db entry lines into a dict"""
	line = fp.readline()
	#print "==================================================="
	#print line,
	try:
		top_title, value = line.strip().split(':', 1)
	except ValueError:
		while line == '\n':
			line = fp.readline()
		if line == '# EOF\n':
			return None
	value = value.strip()
	#print top_title,value

	ret = {}
	ret[top_title] = value
	while True:
		line = fp.readline()
		#print line.strip()
		try:
			title, value = line.strip().split(':', 1)
			last_title = title
			value = value.strip()
			ret[title] = value
		except ValueError:
			if line == '\n':
				return top_title, ret
			ret[last_title] += '\n'
			ret[last_title] += line.strip()

def get_answer(instr, instr_out, prompt):
	try:
		ans = instr.next().strip()
		print prompt + ans
	except StopIteration:
		ans = raw_input(prompt)
	instr_out.write(ans + '\n')
	return ans

def get_type(table, col, instr, instr_out):
	print '\n==> (' + table + '): ' + col
	types = ['char', 'varchar', 'text', 'boolean', 'smallint', 'integer', 'bigint', 'date', 'time', 'cidr', 'inet', 'macaddr']
	for t in range(len(types)):
		print str(t) + ': ' + types[t]
	ans = get_answer(instr, instr_out, 'Choose a type, or type one in: ')
	if ans.isdigit():
		if ans in ["0", "1"]:
			size = get_answer(instr, instr_out, 'What size ' + types[int(ans)] + '? ')
			return types[int(ans)] + '(' + size + ')'
		return types[int(ans)]
	return ans

def enter(entry):
	"""Add an entry to the postgres db"""
	print entry
	exit()

def go():
	if len(sys.argv) != 2:
		usage()

	try:
		instr_in = open('/tmp/convert_in', 'r')
		print "Found instructions from a previous run; would you"
		print "like to repeat your previous session? (/tmp/convert_in)"
		ans = raw_input('Repeat all but last instruction? [Y/n]: ')
		if ans and ans[0].lower() == 'n':
			raise IOError
	except IOError:
		instr_in = open('/dev/null')
	instr = instr_in.readlines()
	instr = instr[:-1]
	instr = instr.__iter__()
	instr_out = open('/tmp/convert_in', 'w')

	db_file = open(sys.argv[1], 'r')
	db_file.close()
	os.system("sed -i 's/\t/ /g' " + sys.argv[1])
	db_file = open(sys.argv[1], 'r')
	db_name = sys.argv[1].split('/')[-1].split('.')[0]

	tables = template_parse('templates.c')

	conn = pg.connect()
	try:
		conn.query('CREATE DATABASE ' + db_name)
	except pg.ProgrammingError as e:
		print e
		print "Query failed; proper error handling is hard at the moment"
		print "Assuming the error is the database exists, if that is not"
		print "the error, then don't continue"
		print
		print "Continuing will drop " + db_name + " if it exists"
		ans = raw_input("Continue? [y/N] ")
		if ans == '' or ans[0].lower() != 'y':
			exit()
		conn.query('DROP DATABASE ' + db_name)
		conn.query('CREATE DATABASE ' + db_name)
	conn = pg.connect(db_name)

	print
	print "=== IRRD DB does not understand types, you need to define them ==="
	print

	for table,cols in tables.iteritems():
		print table
		print cols
		table = table.replace('-', '_')
		print "\nTable: " + table
		q = 'CREATE TABLE "' + table + '" ('
		for col in cols:
			t = get_type(table, col, instr, instr_out)
			q += col.replace('-', '_') + ' ' + t.replace('-', '_')
			if cols[col][0] == 'mandatory':
				q += " NOT NULL"
			elif cols[col][0] == 'optional':
				pass
			else:
				print "Unknown Option for optional / mandatory field"
				raise ValueError
			if cols[col][1] == 'multiple':
				pass
			elif cols[col][1] == 'single':
				pass
			else:
				print "Unkown Option for multiple / single field"
				raise ValueError
			if cols[col][2] == []:
				pass
			elif cols[col][2] == 'primary/look-up key':
				q += ", PRIMARY KEY (" + col.replace('-', '_') + ")"
			else:
				print "Unknown Option for key field"
				raise ValueError
			q += ","
		q = q[:-1] + ');'
		print q
		conn.query(q)

	entry = db_parse(db_file)
	while entry:
		enter(entry)
		entry = db_parse(db_file)

if __name__ == '__main__':
	go()

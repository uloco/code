#!/usr/bin/env php
<?php
const LDAP_URI = "ldap://ldap.cluenet.org";

const FILTER = "(&
	(objectClass=posixAccount)
	(!(objectClass=suspendedUser))
	(clueSshPubKey=*)
)";

$starttls = false;
$optin = true;

$argv0 = array_shift($argv);

openlog("fetch-keys", LOG_PERROR|LOG_PID, LOG_DAEMON);

putenv("LDAPCONF=".getenv("HOME")."/cluenet/ldap.conf");
$conn = ldap_connect(LDAP_URI);
if (!$conn) {
	syslog(LOG_ERR, "LDAP connection failed");
	exit(1);
}
if (!ldap_set_option($conn, LDAP_OPT_PROTOCOL_VERSION, 3)) {
	syslog(LOG_ERR, "upgrade to LDAPv3 failed");
	exit(1);
}
if ($starttls and !ldap_start_tls($conn)) {
	syslog(LOG_ERR, "TLS negotiation failed: ".ldap_error($conn));
	exit(1);
}
if (!ldap_bind($conn, null, null)) {
	syslog(LOG_ERR, "anonymous bind failed: ".ldap_error($conn));
	exit(1);
}

$search = ldap_list($conn, "ou=people,dc=cluenet,dc=org", FILTER,
	array("uid", "uidNumber", "homeDirectory", "clueSshPubKey"));
if (!$search) {
	syslog(LOG_ERR, "search failed: ".ldap_error($conn));
	exit(1);
}

$num_res = ldap_count_entries($conn, $search);
syslog(LOG_INFO, "found $num_res accounts with keys");

for ($entry = ldap_first_entry($conn, $search);
		$entry != false;
		$entry = ldap_next_entry($conn, $entry)) {

	$values = ldap_get_attributes($conn, $entry);
	$user = $values["uid"][0];
	$uid = (int) $values["uidNumber"][0];
	$home = $values["homeDirectory"][0];
	$keys = $values["clueSshPubKey"];

	syslog(LOG_DEBUG, "processing $user [$uid] (".count($keys)." keys)");

	if ($optin and !file_exists("$home/.ssh/ldap_autofetch")) {
		syslog(LOG_INFO, "optin: skipping $user");
		continue;
	}

	$file = "$home/.ssh/authorized_keys";
	$fh = fopen($file, "w");
	if (!$fh) {
		syslog(LOG_NOTICE, "failed to open $file");
		continue;
	}

	fwrite($fh, "# updated ".date("r")." from LDAP\n");
	foreach ($keys as $key) {
		fwrite($fh, "$key\n");
	}
	syslog(LOG_DEBUG, "wrote ".count($keys)." keys");

	$local = "$file.local";
	if (is_file($local)) {
		$local_fh = fopen($local, "r");
		if (!$local_fh) {
			syslog(LOG_NOTICE, "failed to open $local");
		} else {
			syslog(LOG_DEBUG, "copying keys from $local");
			fwrite($fh, "# local keys\n");
			while (($buf = fread($local_fh, 4096)) !== false) {
				fwrite($fh, $buf);
			}
			fclose($local_fh);
		}
	}

	fclose($fh);
}

ldap_unbind($conn);
closelog();


<?php

require_once "PEAR.php";
require_once "PEAR/Frontend/CLI.php";
require_once "PEAR/Builder.php";

PEAR::setErrorHandling(PEAR_ERROR_DIE, "%s\n");

$ui = new PEAR_Frontend_CLI;
$builder = &new PEAR_Builder($ui);
$builder->build($argv[1], 'buildCallback');

function buildCallback($type, $data) {
    print rtrim($data) . "\n";
}

?>
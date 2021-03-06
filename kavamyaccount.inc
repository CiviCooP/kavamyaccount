<?php

class kavamyaccount_helper {
  public $accountFields = [];
  public $isKavaLid = TRUE;

  private $contactID = 0;
  private $achternaam;
  private $voornaam;
  private $geboortedatum;
  private $rizivnummer;
  private $ordenummer;
  private $apotheken = [];
  private $apotheekFuncties = [];
  private $apotheekTeam = [];
  private $abonnementen = [];

  public function __construct() {
    // get civicrm contact id
    $this->getContactID();

    // opvragen van enkele "eenvoudige" velden
    $this->getNameAndSomeOtherFields();

    // apotheken en functies
    $this->getFunction();

    if (count($this->apotheken) == 0) {
      $this->apotheken = ['onbekend'];
    }
    if (count($this->apotheekFuncties) == 0) {
      $this->apotheekFuncties = ['onbekend'];
    }

    // structuur:
    //   index = unieke naam in kleine letters zonder accenten, spaties...
    //   waarde = array met Label, Waarde
    $this->accountFields['voornaam'] = ['Voornaam', $this->voornaam];
    $this->accountFields['achternaam'] = ['Achternaam', $this->achternaam];
    $this->accountFields['geboortedatum'] = ['Geboortedatum', $this->geboortedatum];
    $this->accountFields['rizivnummer'] = ['RIZIV-nummer', $this->rizivnummer];
    //$this->accountFields['ordenummer'] = ['Nummer Orde der apothekers', $this->ordenummer];
    $this->accountFields['prive-adres'] = ['Privé adres', $this->getHomeAddress()];
    $this->accountFields['verzendadres'] = ['Verzendadres', $this->getSendingAddress()];
    $this->accountFields['phone'] = ['Telefoon', $this->getPhone()];
    $this->accountFields['email'] = ['E-mail', $this->getEmail()];
    $this->accountFields['soort-lidmaatschap'] = ['Soort lidmaatschap', $this->getMembership($this->contactID)];
    $this->accountFields['functie'] = ['Functie', implode(', ', $this->apotheekFuncties)];
    $this->accountFields['werkgever'] = ['Werkgever', implode(', ', $this->apotheken)];
    if ($this->getELZ() != '') {
       $this->accountFields['elzdisplay'] = ['Eerstelijnszone', $this->getELZ()];
    }
    $this->accountFields['niewsbrieven'] = ['Nieuwsbrieven', $this->getNewsletters()];

    // only for (co-)titularis
    if (count($this->apotheekTeam) > 0) {
      $url = base_path() . 'sites/all/modules/custom/kavamyaccount/questionmark.png';
      $helpText = '<img src="'. $url . '" title="Het kennen van uw adjunct, FTA... kan nuttig zijn voor (o.a.) opleidingen en inschrijvingen op het niveau van de apotheek én voor activiteiten op maat van bv. uw apotheekassistent. Geef via de `Wijziging doorgeven` knop de correcte gegevens door. Dat doet u pas na toestemming van de betrokken medewerker om diens gegevens (het gaat dan concreet om voornaam, naam, mailadres en functie) met ons te delen. Uiteraard gaan we hiermee op een correcte en GDPR-vriendelijke manier om, zie ook onze privacy voorwaarden en GDPR-verklaring."> ';
      $this->accountFields['apotheekteam'] = ['Mijn apotheekteam', $helpText . implode('<br>', $this->apotheekTeam)];
    }
    if (count($this->abonnementen) > 0) {
      $this->accountFields['abonnementen'] = ['Mijn abonnementen', implode('<br>', $this->abonnementen)];
    }
  }

  private function getContactID() {
    global $user;

    if ($user->uid) {
      // get civicrm contact id
      $result = civicrm_api3('UFMatch', 'get', [
        'sequential' => 1,
        'uf_id' => $user->uid,
      ]);

      if ($result['count'] > 0) {
        $this->contactID = $result['values'][0]['contact_id'];
      }
    }
  }

  private function getNameAndSomeOtherFields() {
    $sql = "
      SELECT
        c.first_name,
        c.last_name,
        date_format(c.birth_date, '%d-%m-%Y') birth_date,
        ca.riziv_nummer_14 riziv_nummer,
        ca.inschrijvingsnr_orde_74 orde_nummer
      FROM
        civicrm_contact c
      left outer join
        civicrm_value_contact_apotheker ca on c.id = ca.entity_id
      WHERE
        c.id = %1
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      $this->voornaam = $dao->first_name;
      $this->achternaam = $dao->last_name;
      $this->geboortedatum = $dao->birth_date;
      $this->rizivnummer = $dao->riziv_nummer;
      $this->ordenummer = $dao->orde_nummer;
    }
  }

  private function getEmail() {
    // get the primary email address
    $sql = "
      SELECT
        email
      FROM
        civicrm_email
      WHERE
        contact_id = %1
      AND
        is_primary = 1
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      return $dao->email;
    }
    else {
      return '';
    }
  }

  private function getPhone() {
    // get the primary phone
    $sql = "
      SELECT
        phone
      FROM
        civicrm_phone
      WHERE
        contact_id = %1
      AND
        is_primary = 1
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      return $dao->phone;
    }
    else {
      return '';
    }
  }

  private function getSendingAddress() {
    // get the primary address
    $sql = "
      SELECT
        concat(street_address, ', ', postal_code, ' ', city) address
      FROM
        civicrm_address
      WHERE
        contact_id = %1
      AND
        is_primary = 1
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      return $dao->address;
    }
    else {
      return 'idem';
    }
  }

  private function getHomeAddress() {
    // get the home address (= NOT the primary address)
    // we return the first one (if there are many)
    $sql = "
      SELECT
        concat(street_address, ', ', postal_code, ' ', city) address
      FROM
        civicrm_address
      WHERE
        contact_id = %1
      AND
        is_primary = 0
      AND
        location_type_id in (1, 3, 4)
      order by
        location_type_id
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      return $dao->address;
    }
    else {
      return 'idem';
    }
  }

  private function getMembership($contactId) {
    $sql = "
      SELECT
        label_a_b membership
      FROM
        civicrm_relationship r
      INNER JOIN
        civicrm_relationship_type rt on r.relationship_type_id = rt.id
      WHERE
        relationship_type_id in (42, 43, 44, 46, 47, 48, 49)
      AND
        start_date <= NOW()
      AND
        end_date >= NOW()
      AND
        contact_id_a = %1
    ";
    $sqlParams = [
      1 => [$contactId, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      // replace "is " and " van"
      return str_replace(' van', '', str_replace('is ', '', $dao->membership));
    }
    else {
      // not a member, add link to membership page
      $this->isKavaLid = FALSE;
      return 'geen lid';
    }
  }

  private function getFunction() {
    // inconsistent use of name_a_b vs. name_b_a
    // we can't retrieve all at once!
    $this->getFunctionDetails('BA', 'titularis', 35, TRUE);
    $this->getFunctionDetails('BA', 'co-titularis', 41, TRUE);
    $this->getFunctionDetails('AB', 'adjunct', 37, FALSE);
    $this->getFunctionDetails('AB', 'plaatsvervangend apotheker', 38, FALSE);
    $this->getFunctionDetails('AB', 'farmaceutisch technisch assistent', 53, FALSE);
  }

  private function getFunctionDetails($direction, $title, $relationshipID, $getExtraPharmacyInfo) {
    if ($direction == 'AB') {
      $person = 'contact_id_a';
      $employer = 'contact_id_b';
    }
    else {
      $person = 'contact_id_b';
      $employer = 'contact_id_a';
    }

    $sql = "
      SELECT
        c.id,
        c.display_name apotheek,
        au.apb_nummer_43 apb_nummer
      FROM
        civicrm_relationship r
      INNER JOIN
        civicrm_contact c on c.id = r.{$employer}
      LEFT OUTER JOIN
        civicrm_value_contact_apotheekuitbating au on au.entity_id = c.id
      WHERE
        relationship_type_id = %2
      AND
        start_date <= NOW()
      AND
        end_date >= NOW()
      AND
        r.{$person} = %1
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
      2 => [$relationshipID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    $found = FALSE;
    while ($dao->fetch()) {
      $found = TRUE;

      $apoName = $dao->apotheek;
      if ($dao->apb_nummer) {
        $apoName .= ' (APB-nr. ' . $dao->apb_nummer . ')';
      }

      $this->apotheken[] = $apoName;

      if ($getExtraPharmacyInfo) {
        // get team members
        $this->getTeamMembers($dao->id, 'BA', 'titularis', 35);
        $this->getTeamMembers($dao->id,'BA', 'co-titularis', 41);
        $this->getTeamMembers($dao->id,'AB', 'adjunct', 37);
        $this->getTeamMembers($dao->id,'AB', 'plaatsvervangend apotheker', 38);
        $this->getTeamMembers($dao->id,'AB', 'farmaceutisch technisch assistent', 53);

        // get subscriptions
        $this->getSubscriptions($dao->id);
      }
    }

    if ($found) {
      $this->apotheekFuncties[] = $title;
    }
  }

  private function getNewsletters() {
    $nwslttrList = [];

    // get newsletter subscriptions
    $sql = "
      select
        kava_communicatiediensten_253 newsletter_subscriptions
      from
        civicrm_value_kava_communic_76
      where
        entity_id = %1
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch() && $dao->newsletter_subscriptions) {
      // concert the newsletter subsc. to an array
      $subscriptions = explode(CRM_Core_DAO::VALUE_SEPARATOR, $dao->newsletter_subscriptions);
    }
    else {
      $subscriptions = [];
    }

    // get newsletter names
    $result = civicrm_api3('OptionValue', 'get', [
      'sequential' => 1,
      'option_group_id' => "kava_communicatiediensten_20180829112251",
      'is_active' => 1,
    ]);

    // loop over the newsletters
    foreach ($result['values'] as $nwslttr) {
      // check if the person is subscribed to that newsletter
      if (in_array($nwslttr['value'], $subscriptions)) {
        $nwslttrList[] = $nwslttr['label'] . ': ja';
      }
      else {
        $nwslttrList[] = $nwslttr['label'] . ': nee';
      }
    }

    return implode('<br>', $nwslttrList);
  }

  private function getELZ() {
    $elzdisplay = '';

    $sql = "
	    SELECT
	      elz.display_name displayname
	    FROM
	      civicrm_contact contact
	    JOIN
	      civicrm_relationship  rel ON contact.id=rel.contact_id_a
	    JOIN
	      civicrm_contact elz ON elz.id=contact_id_b
	    WHERE
	      relationship_type_id = 109 AND rel.end_date > CURRENT_DATE and contact.id = %1;
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      $elzdisplay = $dao->displayname;
    }
    return $elzdisplay;
  }

  private function getTeamMembers($apoId, $direction, $title, $relationshipID) {
    if ($direction == 'AB') {
      $person = 'contact_id_a';
      $employer = 'contact_id_b';
    }
    else {
      $person = 'contact_id_b';
      $employer = 'contact_id_a';
    }

    $sql = "
      SELECT
        p.id,
        concat(p.first_name, ' ', p.last_name) team_member
      FROM
        civicrm_relationship r
      INNER JOIN
        civicrm_contact p on p.id = r.{$person}
      WHERE
        r.relationship_type_id = %2
      AND
        r.start_date <= NOW()
      AND
        r.end_date >= NOW()
      AND
        r.{$employer} = %1
    ";
    $sqlParams = [
      1 => [$apoId, 'Integer'],
      2 => [$relationshipID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    while ($dao->fetch()) {
      // get the KAVA membership of this person
      $member = $this->getMembership($dao->id);

      $this->apotheekTeam[] = $dao->team_member . " ($title - $member)";
    }
  }

  private function getSubscriptions($apoId) {
    $sql = "
      SELECT
        count(m.id)
      FROM
        civicrm_membership m
      WHERE
        m.membership_type_id = %2
      AND
        m.start_date <= NOW()
      AND
        m.end_date >= NOW()
      AND
        m.contact_id = %1
    ";

    // see if there's an AFT subscription (id = 2)
    $sqlParams = [
      1 => [$apoId, 'Integer'],
      2 => [2, 'Integer'],
    ];
    $n = CRM_Core_DAO::singleValueQuery($sql, $sqlParams);
    if ($n) {
      $this->abonnementen[] = 'AFT (ja)';
    }
    else {
      $this->abonnementen[] = 'AFT (nee, <a href="https://www.kava.be/aft">neem abonnement</a>)';
    }

    // see if there's a GMW subscription (id = 7)
    $sqlParams = [
      1 => [$apoId, 'Integer'],
      2 => [7, 'Integer'],
    ];
    $n = CRM_Core_DAO::singleValueQuery($sql, $sqlParams);
    if ($n) {
      $this->abonnementen[] = 'Geneesmiddelenwijzer (ja)';
    }
    else {
      $this->abonnementen[] = 'Geneesmiddelenwijzer (nee, <a href="https://www.kava.be/Bestelformuliergeneesmiddelenwijzerjaarabonnement">neem abonnement</a>)';
    }
  }
}

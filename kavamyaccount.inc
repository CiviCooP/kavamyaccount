<?php

class kavamyaccount_helper {
  public $accountFields = [];
  public $isKavaLid = TRUE;

  private $contactID = 0;
  private $achternaam;
  private $voornaam;
  private $geboortedatum;
  private $apotheken = [];
  private $apotheekFuncties = [];

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
    $this->accountFields['prive-adres'] = ['Privé adres', $this->getHomeAddress()];
    $this->accountFields['verzendadres'] = ['Verzendadres', $this->getSendingAddress()];
    $this->accountFields['phone'] = ['Telefoon', $this->getPhone()];
    $this->accountFields['email'] = ['E-mail', $this->getEmail()];
    $this->accountFields['soort-lidmaatschap'] = ['Soort lidmaatschap', $this->getMembership()];
    $this->accountFields['functie'] = ['Functie', implode(', ', $this->apotheekFuncties)];
    $this->accountFields['werkgever'] = ['Werkgever', implode(', ', $this->apotheken)];
    $this->accountFields['elz-zone'] = ['ELZ-zone', $this->getELZone()];
    $this->accountFields['niewsbrieven'] = ['Nieuwsbrieven', $this->getNewsletters()];
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
        first_name,
        last_name,
        date_format(birth_date, '%d-%m-%Y') birth_date
      FROM
        civicrm_contact
      WHERE
        id = %1
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      $this->voornaam = $dao->first_name;
      $this->achternaam = $dao->last_name;
      $this->geboortedatum = $dao->birth_date;
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

  private function getMembership() {
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
      1 => [$this->contactID, 'Integer'],
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
    $this->getFunctionDetails('BA', 'titularis', 35);
    $this->getFunctionDetails('BA', 'co-titularis', 41);
    $this->getFunctionDetails('AB', 'adjunct', 37);
    $this->getFunctionDetails('AB', 'plaatsvervangend apotheker', 38);
    $this->getFunctionDetails('AB', 'farmaceutisch technisch assistent', 53);
  }

  private function getFunctionDetails($direction, $title, $relationshipID) {
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
    }

    if ($found) {
      $this->apotheekFuncties[] = $title;
    }
  }

  private function getELZone() {
    $sql = "
      select
        g.title elz_zone
      from
        civicrm_group g
      inner join 
        civicrm_group_contact gc on g.id = gc.group_id
      where
        gc.contact_id = %1
      and 
        g.title like 'ELZ %'
    ";
    $sqlParams = [
      1 => [$this->contactID, 'Integer'],
    ];

    $dao = CRM_Core_DAO::executeQuery($sql, $sqlParams);
    if ($dao->fetch()) {
      // strip chars
      $zone = $dao->elz_zone;
      $strip = ['ELZ ', ' Medewerkers', ' Titularis'];
      foreach ($strip as $toStrip) {
        $zone = str_replace($toStrip, '', $zone);
      }

      return $zone;
    }
    else {
      return 'onbekend';
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
    }

    return implode('<br>', $nwslttrList);
  }


}

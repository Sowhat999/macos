//   NOTE: this is a machine generated file--editing not recommended
//
// sm_x411mtsas.h - class definitions for ASN.1 module MTSAbstractService
//
//   This file was generated by snacc on Mon Apr 22 22:34:19 2002
//   UBC snacc by Mike Sample
//   A couple of enhancements made by IBM European Networking Center

#ifndef _sm_x411mtsas_h_
#define _sm_x411mtsas_h_


//------------------------------------------------------------------------------
// class declarations:

class CountryName;
class AdministrationDomainName;
class PrivateDomainName;
class PersonalName;
class DomainDefinedAttribute;
class ExtensionAttribute;
class TeletexDomainDefinedAttribute;
class OrganizationUnitNames;
class DomainDefinedAttributes;
class ExtensionAttributes;
class StandardAttributes;
class ORAddress;
class TeletexPersonalName;
class TeletexOrganizationUnitNames;
class TeletexDomainDefinedAttributes;

//------------------------------------------------------------------------------
// class definitions:

typedef enum MTSAbstractServiceAnyId
{

} MTSAbstractServiceAnyId;


class CountryName: public AsnType
{
public:
  enum ChoiceIdEnum
  {
     x121_dcc_codeCid = 0,
     iso_3166_alpha2_codeCid = 1
  };

  enum ChoiceIdEnum	choiceId;
  union
  {
     NumericString		*x121_dcc_code;
     PrintableString		*iso_3166_alpha2_code;
  };


			CountryName();
			CountryName (const CountryName &);
  virtual		~CountryName();

  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  CountryName		&operator = (const CountryName &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void			Print (ostream &os) const;
};


class AdministrationDomainName: public AsnType
{
public:
  enum ChoiceIdEnum
  {
     numericCid = 0,
     printableCid = 1
  };

  enum ChoiceIdEnum	choiceId;
  union
  {
     NumericString		*numeric;
     PrintableString		*printable;
  };


			AdministrationDomainName();
			AdministrationDomainName (const AdministrationDomainName &);
  virtual		~AdministrationDomainName();

  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  AdministrationDomainName		&operator = (const AdministrationDomainName &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void			Print (ostream &os) const;
};


/* NumericString (SIZE (1..ub-x121-address-length)) */
typedef NumericString X121Address;

/* PrintableString (SIZE (1..ub-terminal-id-length)) */
typedef PrintableString TerminalIdentifier;

class PrivateDomainName: public AsnType
{
public:
  enum ChoiceIdEnum
  {
     numericCid = 0,
     printableCid = 1
  };

  enum ChoiceIdEnum	choiceId;
  union
  {
     NumericString		*numeric;
     PrintableString		*printable;
  };


			PrivateDomainName();
			PrivateDomainName (const PrivateDomainName &);
  virtual		~PrivateDomainName();

  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  PrivateDomainName		&operator = (const PrivateDomainName &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void			Print (ostream &os) const;
};


/* PrintableString (SIZE (1..ub-organization-name-length)) */
typedef PrintableString OrganizationName;

/* NumericString (SIZE (1..ub-numeric-user-id-length)) */
typedef NumericString NumericUserIdentifier;

class PersonalName: public AsnType
{
public:
  PrintableString		surname;
  PrintableString		*given_name;
  PrintableString		*initials;
  PrintableString		*generation_qualifier;

			PersonalName();
			PersonalName (const PersonalName &);
  virtual		~PersonalName();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  PersonalName		&operator = (const PersonalName &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void			Print (ostream &os) const;
};


/* PrintableString (SIZE (1..ub-organizational-unit-name-length)) */
typedef PrintableString OrganizationUnitName;

class DomainDefinedAttribute: public AsnType
{
public:
  PrintableString		type;
  PrintableString		value;

			DomainDefinedAttribute();
			DomainDefinedAttribute (const DomainDefinedAttribute &);
  virtual		~DomainDefinedAttribute();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  DomainDefinedAttribute		&operator = (const DomainDefinedAttribute &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void		Print (ostream &os) const;
};


class ExtensionAttribute: public AsnType
{
public:
  AsnInt		extension_attribute_type;
  AsnAny		extension_attribute_value;

			ExtensionAttribute();
			ExtensionAttribute (const ExtensionAttribute &);
  virtual		~ExtensionAttribute();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  ExtensionAttribute		&operator = (const ExtensionAttribute &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void		Print (ostream &os) const;
};


/* TeletexString (SIZE (1..ub-organizational-unit-name-length)) */
typedef TeletexString TeletexOrganizationalUnitName;

class TeletexDomainDefinedAttribute: public AsnType
{
public:
  TeletexString		type;
  TeletexString		value;

			TeletexDomainDefinedAttribute();
			TeletexDomainDefinedAttribute (const TeletexDomainDefinedAttribute &);
  virtual		~TeletexDomainDefinedAttribute();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  TeletexDomainDefinedAttribute		&operator = (const TeletexDomainDefinedAttribute &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void		Print (ostream &os) const;
};


/* X121Address */
typedef X121Address NetworkAddress;

class OrganizationUnitNames: public AsnType
{
protected:
  unsigned long int	count;
  struct AsnListElmt
  {
    AsnListElmt	*next;
    AsnListElmt	*prev;
    OrganizationUnitName	*elmt;
  }			*first, *curr, *last;

public:
			OrganizationUnitNames() { count = 0; first = curr = last = NULL; }
			OrganizationUnitNames (const OrganizationUnitNames &);
  virtual		~OrganizationUnitNames();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  OrganizationUnitNames		&operator = (const OrganizationUnitNames &);
  void		SetCurrElmt (unsigned long int index);
  unsigned long int	GetCurrElmtIndex();
  void		SetCurrToFirst() { curr = first; }
  void		SetCurrToLast()  { curr = last; }
  // reading member fcns
  int			Count() const	{ return count; }
  // NOTE: if your compiler complains about these NULLs, its definition of NULL is broken (and you better change it there!)
  OrganizationUnitName	*First() const	{ return count > 0 ? first->elmt : NULL; }
  OrganizationUnitName	*Last() const	{ return count > 0 ? last->elmt : NULL; }
  OrganizationUnitName	*Curr() const	{ return curr ? curr->elmt : NULL; }
  OrganizationUnitName	*Next() const	{ return curr && curr->next ? curr->next->elmt : NULL; }
  OrganizationUnitName	*Prev() const	{ return curr && curr->prev ? curr->prev->elmt : NULL; }

  // routines that move the curr elmt
  OrganizationUnitName	*GoNext() { if (curr) curr = curr->next; return Curr(); }
  OrganizationUnitName	*GoPrev() { if (curr) curr = curr->prev; return Curr(); }

  // write & alloc fcns - returns new elmt
  OrganizationUnitName	*Append();  // add elmt to end of list
  OrganizationUnitName	*Prepend(); // add elmt to beginning of list
  OrganizationUnitName	*InsertBefore(); //insert elmt before current elmt
  OrganizationUnitName	*InsertAfter(); //insert elmt after current elmt

  // write & alloc & copy - returns list after copying elmt
  OrganizationUnitNames	&AppendCopy (OrganizationUnitName &elmt);  // add elmt to end of list
  OrganizationUnitNames	&PrependCopy (OrganizationUnitName &elmt); // add elmt to beginning of list
  OrganizationUnitNames	&InsertBeforeAndCopy (OrganizationUnitName &elmt); //insert elmt before current elmt
  OrganizationUnitNames	&InsertAfterAndCopy (OrganizationUnitName &elmt); //insert elmt after current elmt

  // removing the current elmt from the list
  void		RemoveCurrFromList();

  // encode and decode routines    
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS
  void		Print (ostream &os) const;
};


class DomainDefinedAttributes: public AsnType
{
protected:
  unsigned long int	count;
  struct AsnListElmt
  {
    AsnListElmt	*next;
    AsnListElmt	*prev;
    DomainDefinedAttribute	*elmt;
  }			*first, *curr, *last;

public:
			DomainDefinedAttributes() { count = 0; first = curr = last = NULL; }
			DomainDefinedAttributes (const DomainDefinedAttributes &);
  virtual		~DomainDefinedAttributes();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  DomainDefinedAttributes		&operator = (const DomainDefinedAttributes &);
  void		SetCurrElmt (unsigned long int index);
  unsigned long int	GetCurrElmtIndex();
  void		SetCurrToFirst() { curr = first; }
  void		SetCurrToLast()  { curr = last; }
  // reading member fcns
  int			Count() const	{ return count; }
  // NOTE: if your compiler complains about these NULLs, its definition of NULL is broken (and you better change it there!)
  DomainDefinedAttribute	*First() const	{ return count > 0 ? first->elmt : NULL; }
  DomainDefinedAttribute	*Last() const	{ return count > 0 ? last->elmt : NULL; }
  DomainDefinedAttribute	*Curr() const	{ return curr ? curr->elmt : NULL; }
  DomainDefinedAttribute	*Next() const	{ return curr && curr->next ? curr->next->elmt : NULL; }
  DomainDefinedAttribute	*Prev() const	{ return curr && curr->prev ? curr->prev->elmt : NULL; }

  // routines that move the curr elmt
  DomainDefinedAttribute	*GoNext() { if (curr) curr = curr->next; return Curr(); }
  DomainDefinedAttribute	*GoPrev() { if (curr) curr = curr->prev; return Curr(); }

  // write & alloc fcns - returns new elmt
  DomainDefinedAttribute	*Append();  // add elmt to end of list
  DomainDefinedAttribute	*Prepend(); // add elmt to beginning of list
  DomainDefinedAttribute	*InsertBefore(); //insert elmt before current elmt
  DomainDefinedAttribute	*InsertAfter(); //insert elmt after current elmt

  // write & alloc & copy - returns list after copying elmt
  DomainDefinedAttributes	&AppendCopy (DomainDefinedAttribute &elmt);  // add elmt to end of list
  DomainDefinedAttributes	&PrependCopy (DomainDefinedAttribute &elmt); // add elmt to beginning of list
  DomainDefinedAttributes	&InsertBeforeAndCopy (DomainDefinedAttribute &elmt); //insert elmt before current elmt
  DomainDefinedAttributes	&InsertAfterAndCopy (DomainDefinedAttribute &elmt); //insert elmt after current elmt

  // removing the current elmt from the list
  void		RemoveCurrFromList();

  // encode and decode routines    
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS
  void		Print (ostream &os) const;
};


class ExtensionAttributes: public AsnType
{
protected:
  unsigned long int	count;
  struct AsnListElmt
  {
    AsnListElmt	*next;
    AsnListElmt	*prev;
    ExtensionAttribute	*elmt;
  }			*first, *curr, *last;

public:
			ExtensionAttributes() { count = 0; first = curr = last = NULL; }
			ExtensionAttributes (const ExtensionAttributes &);
  virtual		~ExtensionAttributes();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  ExtensionAttributes		&operator = (const ExtensionAttributes &);
  void		SetCurrElmt (unsigned long int index);
  unsigned long int	GetCurrElmtIndex();
  void		SetCurrToFirst() { curr = first; }
  void		SetCurrToLast()  { curr = last; }
  // reading member fcns
  int			Count() const	{ return count; }
  // NOTE: if your compiler complains about these NULLs, its definition of NULL is broken (and you better change it there!)
  ExtensionAttribute	*First() const	{ return count > 0 ? first->elmt : NULL; }
  ExtensionAttribute	*Last() const	{ return count > 0 ? last->elmt : NULL; }
  ExtensionAttribute	*Curr() const	{ return curr ? curr->elmt : NULL; }
  ExtensionAttribute	*Next() const	{ return curr && curr->next ? curr->next->elmt : NULL; }
  ExtensionAttribute	*Prev() const	{ return curr && curr->prev ? curr->prev->elmt : NULL; }

  // routines that move the curr elmt
  ExtensionAttribute	*GoNext() { if (curr) curr = curr->next; return Curr(); }
  ExtensionAttribute	*GoPrev() { if (curr) curr = curr->prev; return Curr(); }

  // write & alloc fcns - returns new elmt
  ExtensionAttribute	*Append();  // add elmt to end of list
  ExtensionAttribute	*Prepend(); // add elmt to beginning of list
  ExtensionAttribute	*InsertBefore(); //insert elmt before current elmt
  ExtensionAttribute	*InsertAfter(); //insert elmt after current elmt

  // write & alloc & copy - returns list after copying elmt
  ExtensionAttributes	&AppendCopy (ExtensionAttribute &elmt);  // add elmt to end of list
  ExtensionAttributes	&PrependCopy (ExtensionAttribute &elmt); // add elmt to beginning of list
  ExtensionAttributes	&InsertBeforeAndCopy (ExtensionAttribute &elmt); //insert elmt before current elmt
  ExtensionAttributes	&InsertAfterAndCopy (ExtensionAttribute &elmt); //insert elmt after current elmt

  // removing the current elmt from the list
  void		RemoveCurrFromList();

  // encode and decode routines    
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS
  void		Print (ostream &os) const;
};


class StandardAttributes: public AsnType
{
public:
  CountryName		*country_name;
  AdministrationDomainName		*administration_domain_name;
  NetworkAddress		*network_address;
  TerminalIdentifier		*terminal_identifier;
  PrivateDomainName		*private_domain_name;
  OrganizationName		*organization_name;
  NumericUserIdentifier		*numeric_user_identifier;
  PersonalName		*personal_name;
  OrganizationUnitNames		*organizational_unit_names;

			StandardAttributes();
			StandardAttributes (const StandardAttributes &);
  virtual		~StandardAttributes();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  StandardAttributes		&operator = (const StandardAttributes &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void		Print (ostream &os) const;
};


class ORAddress: public AsnType
{
public:
  StandardAttributes		*standard_attributes;
  DomainDefinedAttributes		*domain_defined_attributes;
  ExtensionAttributes		*extension_attributes;

			ORAddress();
			ORAddress (const ORAddress &);
  virtual		~ORAddress();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  ORAddress		&operator = (const ORAddress &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void		Print (ostream &os) const;
};


/* PrintableString (SIZE (1..ub-common-name-length)) */
typedef PrintableString CommonName;

/* TeletexString (SIZE (1..ub-common-name-length)) */
typedef TeletexString TeletexCommonName;

/* TeletexString (SIZE (1..ub-organization-name-length)) */
typedef TeletexString TeletexOrganizationalName;

class TeletexPersonalName: public AsnType
{
public:
  TeletexString		surname;
  TeletexString		*given_name;
  TeletexString		*initials;
  TeletexString		*generation_qualifier;

			TeletexPersonalName();
			TeletexPersonalName (const TeletexPersonalName &);
  virtual		~TeletexPersonalName();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  TeletexPersonalName		&operator = (const TeletexPersonalName &);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  void			Print (ostream &os) const;
};


class TeletexOrganizationUnitNames: public AsnType
{
protected:
  unsigned long int	count;
  struct AsnListElmt
  {
    AsnListElmt	*next;
    AsnListElmt	*prev;
    TeletexOrganizationalUnitName	*elmt;
  }			*first, *curr, *last;

public:
			TeletexOrganizationUnitNames() { count = 0; first = curr = last = NULL; }
			TeletexOrganizationUnitNames (const TeletexOrganizationUnitNames &);
  virtual		~TeletexOrganizationUnitNames();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  TeletexOrganizationUnitNames		&operator = (const TeletexOrganizationUnitNames &);
  void		SetCurrElmt (unsigned long int index);
  unsigned long int	GetCurrElmtIndex();
  void		SetCurrToFirst() { curr = first; }
  void		SetCurrToLast()  { curr = last; }
  // reading member fcns
  int			Count() const	{ return count; }
  // NOTE: if your compiler complains about these NULLs, its definition of NULL is broken (and you better change it there!)
  TeletexOrganizationalUnitName	*First() const	{ return count > 0 ? first->elmt : NULL; }
  TeletexOrganizationalUnitName	*Last() const	{ return count > 0 ? last->elmt : NULL; }
  TeletexOrganizationalUnitName	*Curr() const	{ return curr ? curr->elmt : NULL; }
  TeletexOrganizationalUnitName	*Next() const	{ return curr && curr->next ? curr->next->elmt : NULL; }
  TeletexOrganizationalUnitName	*Prev() const	{ return curr && curr->prev ? curr->prev->elmt : NULL; }

  // routines that move the curr elmt
  TeletexOrganizationalUnitName	*GoNext() { if (curr) curr = curr->next; return Curr(); }
  TeletexOrganizationalUnitName	*GoPrev() { if (curr) curr = curr->prev; return Curr(); }

  // write & alloc fcns - returns new elmt
  TeletexOrganizationalUnitName	*Append();  // add elmt to end of list
  TeletexOrganizationalUnitName	*Prepend(); // add elmt to beginning of list
  TeletexOrganizationalUnitName	*InsertBefore(); //insert elmt before current elmt
  TeletexOrganizationalUnitName	*InsertAfter(); //insert elmt after current elmt

  // write & alloc & copy - returns list after copying elmt
  TeletexOrganizationUnitNames	&AppendCopy (TeletexOrganizationalUnitName &elmt);  // add elmt to end of list
  TeletexOrganizationUnitNames	&PrependCopy (TeletexOrganizationalUnitName &elmt); // add elmt to beginning of list
  TeletexOrganizationUnitNames	&InsertBeforeAndCopy (TeletexOrganizationalUnitName &elmt); //insert elmt before current elmt
  TeletexOrganizationUnitNames	&InsertAfterAndCopy (TeletexOrganizationalUnitName &elmt); //insert elmt after current elmt

  // removing the current elmt from the list
  void		RemoveCurrFromList();

  // encode and decode routines    
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS
  void		Print (ostream &os) const;
};


class TeletexDomainDefinedAttributes: public AsnType
{
protected:
  unsigned long int	count;
  struct AsnListElmt
  {
    AsnListElmt	*next;
    AsnListElmt	*prev;
    TeletexDomainDefinedAttribute	*elmt;
  }			*first, *curr, *last;

public:
			TeletexDomainDefinedAttributes() { count = 0; first = curr = last = NULL; }
			TeletexDomainDefinedAttributes (const TeletexDomainDefinedAttributes &);
  virtual		~TeletexDomainDefinedAttributes();
  virtual AsnType	*Clone() const;

  virtual AsnType	*Copy() const;

  TeletexDomainDefinedAttributes		&operator = (const TeletexDomainDefinedAttributes &);
  void		SetCurrElmt (unsigned long int index);
  unsigned long int	GetCurrElmtIndex();
  void		SetCurrToFirst() { curr = first; }
  void		SetCurrToLast()  { curr = last; }
  // reading member fcns
  int			Count() const	{ return count; }
  // NOTE: if your compiler complains about these NULLs, its definition of NULL is broken (and you better change it there!)
  TeletexDomainDefinedAttribute	*First() const	{ return count > 0 ? first->elmt : NULL; }
  TeletexDomainDefinedAttribute	*Last() const	{ return count > 0 ? last->elmt : NULL; }
  TeletexDomainDefinedAttribute	*Curr() const	{ return curr ? curr->elmt : NULL; }
  TeletexDomainDefinedAttribute	*Next() const	{ return curr && curr->next ? curr->next->elmt : NULL; }
  TeletexDomainDefinedAttribute	*Prev() const	{ return curr && curr->prev ? curr->prev->elmt : NULL; }

  // routines that move the curr elmt
  TeletexDomainDefinedAttribute	*GoNext() { if (curr) curr = curr->next; return Curr(); }
  TeletexDomainDefinedAttribute	*GoPrev() { if (curr) curr = curr->prev; return Curr(); }

  // write & alloc fcns - returns new elmt
  TeletexDomainDefinedAttribute	*Append();  // add elmt to end of list
  TeletexDomainDefinedAttribute	*Prepend(); // add elmt to beginning of list
  TeletexDomainDefinedAttribute	*InsertBefore(); //insert elmt before current elmt
  TeletexDomainDefinedAttribute	*InsertAfter(); //insert elmt after current elmt

  // write & alloc & copy - returns list after copying elmt
  TeletexDomainDefinedAttributes	&AppendCopy (TeletexDomainDefinedAttribute &elmt);  // add elmt to end of list
  TeletexDomainDefinedAttributes	&PrependCopy (TeletexDomainDefinedAttribute &elmt); // add elmt to beginning of list
  TeletexDomainDefinedAttributes	&InsertBeforeAndCopy (TeletexDomainDefinedAttribute &elmt); //insert elmt before current elmt
  TeletexDomainDefinedAttributes	&InsertAfterAndCopy (TeletexDomainDefinedAttribute &elmt); //insert elmt after current elmt

  // removing the current elmt from the list
  void		RemoveCurrFromList();

  // encode and decode routines    
  AsnLen		BEnc (BUF_TYPE b);
  void			BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen		BEncContent (BUF_TYPE b);
  void			BDecContent (BUF_TYPE b, AsnTag tag, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS
  void		Print (ostream &os) const;
};


//------------------------------------------------------------------------------
// externs for value defs

//------------------------------------------------------------------------------

#endif /* conditional include of sm_x411mtsas.h */

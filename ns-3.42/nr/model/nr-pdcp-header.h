// Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only
//
// Author: Manuel Requena <manuel.requena@cttc.es>

#ifndef NR_PDCP_HEADER_H
#define NR_PDCP_HEADER_H

#include "ns3/header.h"

#include <list>

namespace ns3
{

/**
 * \ingroup nr
 * \brief The packet header for the Packet Data Convergence Protocol (PDCP) packets
 *
 * This class has fields corresponding to those in an PDCP header as well as
 * methods for serialization to and deserialization from a byte buffer.
 * It follows 3GPP TS 36.323 Packet Data Convergence Protocol (PDCP) specification.
 */
class NrPdcpHeader : public Header
{
  public:
    /**
     * \brief Constructor
     *
     * Creates a null header
     */
    NrPdcpHeader();
    ~NrPdcpHeader() override;

    /**
     * \brief Set DC bit
     *
     * \param dcBit DC bit to set
     */
    void SetEct(uint8_t dcBit);
    /**
     * \brief Set sequence number
     *
     * \param sequenceNumber sequence number
     */
    void SetSequenceNumber(uint16_t sequenceNumber);

    /**
     * \brief Get DC bit
     *
     * \returns DC bit
     */
    uint8_t GetEct() const;
    /**
     * \brief Get sequence number
     *
     * \returns sequence number
     */
    uint16_t GetSequenceNumber() const;

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    uint8_t m_ect;         ///< the bit that represents a l4s packet
    uint16_t m_sequenceNumber; ///< the sequence number
};

} // namespace ns3

#endif // NR_PDCP_HEADER_H